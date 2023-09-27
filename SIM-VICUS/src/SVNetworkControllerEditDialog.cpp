#include "SVNetworkControllerEditDialog.h"
#include "ui_SVDBNetworkControllerEditWidget.h"

#include "SVDBNetworkControllerTableModel.h"
#include "SVMainWindow.h"
#include "SVDatabaseEditDialog.h"
#include <SVConversions.h>

#include <VICUS_Schedule.h>
#include <VICUS_KeywordListQt.h>
#include <VICUS_BMBlock.h>



SVNetworkControllerEditDialog::SVNetworkControllerEditDialog(QWidget *parent, SVDatabase * db) :
	QDialog(parent),
	m_ui(new Ui::SVDBNetworkControllerEditWidget),
	m_db(db)
{
	m_ui->setupUi(this);

	// setup comboboxes
	m_ui->comboBoxProperty->clear();

	// setup line edits
	m_ui->lineEditKi->setup(0, std::numeric_limits<double>::max(), "Integration Constant", false, true);
	m_ui->lineEditKp->setup(0, std::numeric_limits<double>::max(), "Controller Gain", false, true);
	m_ui->lineEditKp->setFormat('e', 0);
	m_ui->lineEditSetpoint->setup(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max(), "Set Point", false, true);
	m_ui->lineEditMaxControllerResultValue->setup(0, std::numeric_limits<double>::max(), "Max Y", true, true);
	m_ui->lineEditMaxControllerResultValue->setFormat('e', 0);

}

SVNetworkControllerEditDialog::~SVNetworkControllerEditDialog()
{
	delete m_ui;
}

void SVNetworkControllerEditDialog::setup(VICUS::BMBlock *block, VICUS::NetworkController controller, VICUS::NetworkComponent *networkComponent){
	Q_ASSERT(block != nullptr);
	Q_ASSERT(networkComponent != nullptr);

	m_currentController = controller;
	m_currentBlock = block;
	setController(&m_currentController);

	m_ui->comboBoxProperty->clear();

	std::vector<NANDRAD::HydraulicNetworkControlElement::ControlledProperty> availableCtrProps;
	NANDRAD::HydraulicNetworkComponent::ModelType nandradModelType;
	nandradModelType = VICUS::NetworkComponent::nandradNetworkComponentModelType(networkComponent->m_modelType);
	availableCtrProps = NANDRAD::HydraulicNetworkControlElement::availableControlledProperties(nandradModelType);
	for(const auto& prop : availableCtrProps){
		m_ui->comboBoxProperty->addItem(QString("%1")
											.arg(VICUS::KeywordListQt::Description("NetworkController::ControlledProperty", static_cast<int>(prop))), static_cast<int>(prop));

		//ui->controllerComboBox->addItem(QString::fromLatin1(VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", controllerID)));
	}
}

void SVNetworkControllerEditDialog::setController(VICUS::NetworkController *controller) {

	// re-enable all controls
	setEnabled(true);

	// get schedule
	const VICUS::Schedule * setPointSched = m_db->m_schedules[controller->m_idReferences[VICUS::NetworkController::ID_Schedule]];

	// controlled property
	int propIdx = m_ui->comboBoxProperty->findData(controller->m_controlledProperty);
	m_ui->comboBoxProperty->setCurrentIndex(propIdx);
	VICUS::NetworkController::ControlledProperty controlledProperty = VICUS::NetworkController::ControlledProperty(propIdx);

	// define some properties based on current controlled property
	QString controlledPropertyName;
	bool schedulePossible = false;
	bool maxControllerValuePossible = false;
	switch (controlledProperty) {
		case VICUS::NetworkController::CP_TemperatureDifference:
		case VICUS::NetworkController::CP_TemperatureDifferenceOfFollowingElement:{
			controlledPropertyName = tr("Temperature Difference [K]");
			schedulePossible = true;
			maxControllerValuePossible = true;
		} break;
		case VICUS::NetworkController::CP_MassFlux :{
			controlledPropertyName = tr("Mass Flux [kg/s]");
			schedulePossible = true;
			maxControllerValuePossible = true;
		} break;
		case VICUS::NetworkController::CP_PumpOperation :{
			controlledPropertyName = tr("Heat Flux Threshold of Following Element [W]");
			schedulePossible = false;
			maxControllerValuePossible = false;
		} break;
		case VICUS::NetworkController::CP_PressureDifferenceWorstpoint :{
			controlledPropertyName = tr("Pressure Difference at worst point [Pa]");
			schedulePossible = false;
			maxControllerValuePossible = false;
		} break;
		case VICUS::NetworkController::CP_ThermostatValue: {
			controlledPropertyName = tr("Thermostat Value [-]");
			schedulePossible = false;
			maxControllerValuePossible = false;
		} break;
		case VICUS::NetworkController::NUM_CP:
			break;
	}

	// enable states and text
	m_ui->radioButtonFixedSetPoint->setText(tr("Set point ") + controlledPropertyName);
	m_ui->radioButtonSchedule->setEnabled(schedulePossible);
	if (schedulePossible)
		m_ui->radioButtonSchedule->setText(tr("Schedule ") + controlledPropertyName);
	else
		m_ui->radioButtonSchedule->setText("");
	m_ui->toolButtonSchedule->setEnabled(m_ui->radioButtonSchedule->isChecked());
	m_ui->groupBoxMaximumOutput->setEnabled(maxControllerValuePossible);
	m_ui->toolButtonRemoveSchedule->setEnabled(setPointSched != nullptr);

	// update content
	m_ui->lineEditSetpoint->clear();
	m_ui->lineEditSchedule->clear();
	if (controller->m_modelType == VICUS::NetworkController::MT_Constant)
		m_ui->lineEditSetpoint->setValue(controller->m_para[VICUS::NetworkController::setPointType(controlledProperty)].value);
	else if (setPointSched != nullptr)
		m_ui->lineEditSchedule->setText(QtExt::MultiLangString2QString(setPointSched->m_displayName));

	// setup combobox controller type
	m_ui->comboBoxControllerType->clear();
	std::vector<NANDRAD::HydraulicNetworkControlElement::ControllerType> availableCtrTypes =
			NANDRAD::HydraulicNetworkControlElement::availableControllerTypes(
				NANDRAD::HydraulicNetworkControlElement::ControlledProperty(controller->m_controlledProperty));
	for (int i: availableCtrTypes)
		m_ui->comboBoxControllerType->addItem(QString("%1").arg(VICUS::KeywordListQt::Description("NetworkController::ControllerType", i)), i);

	// controller type and parameters
	int typeIdx = m_ui->comboBoxControllerType->findData(controller->m_controllerType);
	m_ui->comboBoxControllerType->setCurrentIndex(typeIdx);
	m_ui->lineEditKp->setEnabled(controller->m_controllerType == VICUS::NetworkController::CT_PController ||
								 controller->m_controllerType == VICUS::NetworkController::CT_PIController );
	m_ui->lineEditKp->setValue(controller->m_para[VICUS::NetworkController::P_Kp].value);
	m_ui->lineEditKi->setEnabled(controller->m_controllerType == VICUS::NetworkController::CT_PIController);
	m_ui->lineEditKi->setValue(controller->m_para[VICUS::NetworkController::P_Ki].value);
	m_ui->lineEditMaxControllerResultValue->setValue(controller->m_maximumControllerResultValue);
	m_ui->radioButtonSchedule->setChecked(controller->m_modelType == VICUS::NetworkController::MT_Scheduled && !schedulePossible);
	m_ui->radioButtonFixedSetPoint->setChecked(controller->m_modelType == VICUS::NetworkController::MT_Constant);
	m_ui->lineEditSetpoint->setEnabled(controller->m_modelType == VICUS::NetworkController::MT_Constant);

	// for built-ins, disable editing/make read-only
	bool isEditable = !controller->m_builtIn;
	m_ui->lineEditSetpoint->setReadOnly(!isEditable);
	m_ui->lineEditKp->setReadOnly(!isEditable);
	m_ui->lineEditKi->setReadOnly(!isEditable);
}

void SVNetworkControllerEditDialog::on_lineEditSetpoint_editingFinished()
{
	if (!m_ui->lineEditSetpoint->isValid())
		return;

	//Q_ASSERT(m_currentController != nullptr);
	VICUS::NetworkController::para_t setPointParaType = VICUS::NetworkController::setPointType(m_currentController.m_controlledProperty);
	if (setPointParaType != VICUS::NetworkController::NUM_P) {
		VICUS::KeywordList::setParameter(m_currentController.m_para, "NetworkController::para_t",
										   setPointParaType,
										   m_ui->lineEditSetpoint->value());
	}

}

void SVNetworkControllerEditDialog::on_lineEditKp_editingFinished()
{
	if (!m_ui->lineEditKp->isValid())
		return;
	//Q_ASSERT(m_currentController != nullptr);
	double val = m_ui->lineEditKp->value();
	if (m_currentController.m_para[VICUS::NetworkController::P_Kp].value < val ||
		m_currentController.m_para[VICUS::NetworkController::P_Kp].value > val ){
		VICUS::KeywordList::setParameter(m_currentController.m_para, "NetworkController::para_t",
										   VICUS::NetworkController::P_Kp, val);

	}
}

void SVNetworkControllerEditDialog::on_lineEditKi_editingFinished()
{
	if (!m_ui->lineEditKi->isValid())
		return;
	//Q_ASSERT(m_currentController != nullptr);
	double val = m_ui->lineEditKi->value();
	if (m_currentController.m_para[VICUS::NetworkController::P_Ki].value < val ||
		m_currentController.m_para[VICUS::NetworkController::P_Ki].value > val ){
		VICUS::KeywordList::setParameter(m_currentController.m_para, "NetworkController::para_t",
										   VICUS::NetworkController::P_Ki, val);

	}
}


void SVNetworkControllerEditDialog::on_radioButtonSchedule_clicked(bool checked)
{
	if (checked && (m_currentController.m_modelType != VICUS::NetworkController::MT_Scheduled)){
		m_currentController.m_modelType = VICUS::NetworkController::MT_Scheduled;

	}
	m_ui->lineEditSetpoint->setEnabled(false);
	m_ui->toolButtonSchedule->setEnabled(checked);
}

void SVNetworkControllerEditDialog::on_radioButtonFixedSetPoint_clicked(bool checked)
{
	if (checked && (m_currentController.m_modelType != VICUS::NetworkController::MT_Constant)){
		m_currentController.m_modelType = VICUS::NetworkController::MT_Constant;

	}
	m_ui->lineEditSetpoint->setEnabled(true);
	m_ui->toolButtonSchedule->setEnabled(false);
}

void SVNetworkControllerEditDialog::on_toolButtonSchedule_clicked()
{
	// open schedule edit dialog in selection mode
	unsigned int newId = SVMainWindow::instance().dbScheduleEditDialog()->select(m_currentController.m_idReferences[VICUS::NetworkController::ID_Schedule]);

	// if dialog was canceled do nothing
	if (newId == VICUS::INVALID_ID)
		return;

	if (m_currentController.m_idReferences[VICUS::NetworkController::ID_Schedule] != newId) {
		m_currentController.m_idReferences[VICUS::NetworkController::ID_Schedule] = newId;

	}
	setController(&m_currentController);

}

void SVNetworkControllerEditDialog::on_comboBoxProperty_activated(int /*index*/)
{
	//Q_ASSERT(m_currentController != nullptr);
	unsigned int val = m_ui->comboBoxProperty->currentData().toUInt();
	if (m_currentController.m_controlledProperty != val) {
		m_currentController.m_controlledProperty = VICUS::NetworkController::ControlledProperty(val);

	}
	setController(&m_currentController);

}

void SVNetworkControllerEditDialog::on_comboBoxControllerType_activated(int /*index*/)
{
	//Q_ASSERT(m_currentController != nullptr);
	unsigned int val = m_ui->comboBoxControllerType->currentData().toUInt();
	if (m_currentController.m_controllerType != val) {
		m_currentController.m_controllerType = VICUS::NetworkController::ControllerType(val);

	}
	setController(&m_currentController);

}

void SVNetworkControllerEditDialog::on_lineEditMaxControllerResultValue_editingFinished()
{
	if (!m_ui->lineEditMaxControllerResultValue->isValid())
		return;
	//Q_ASSERT(m_currentController != nullptr);
	double val = m_ui->lineEditMaxControllerResultValue->value();
	if (m_currentController.m_maximumControllerResultValue < val ||
		m_currentController.m_maximumControllerResultValue > val ){
		m_currentController.m_maximumControllerResultValue = val;

	}
}

void SVNetworkControllerEditDialog::on_toolButtonRemoveSchedule_clicked()
{
	//Q_ASSERT(m_currentController != nullptr);
	m_currentController.m_idReferences[VICUS::NetworkController::ID_Schedule] = VICUS::INVALID_ID;

	setController(&m_currentController);

}


void SVNetworkControllerEditDialog::on_buttonBox_accepted()
{
	emit controllerAccepted(m_currentBlock, m_currentController);
	this->close();
}


void SVNetworkControllerEditDialog::on_buttonBox_rejected()
{
	this->close();
}

