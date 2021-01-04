#include "SVSimulationStartNandrad.h"
#include "ui_SVSimulationStartNandrad.h"

#include <QHBoxLayout>
#include <QMessageBox>


#include <VICUS_Project.h>

#include "SVProjectHandler.h"
#include "SVSettings.h"
#include "SVSimulationPerformanceOptions.h"
#include "SVSimulationLocationOptions.h"
#include "SVSimulationOutputOptions.h"
#include "SVSimulationModelOptions.h"
#include "SVSimulationRunRequestDialog.h"

#include "SVLogFileDialog.h"

SVSimulationStartNandrad::SVSimulationStartNandrad(QWidget *parent) :
	QDialog(parent),
	m_ui(new Ui::SVSimulationStartNandrad)
{
	m_ui->setupUi(this);

	m_ui->comboBoxVerboseLevel->addItem( tr("Minimum"), 0 );
	m_ui->comboBoxVerboseLevel->addItem( tr("Normal"), 1 );
	m_ui->comboBoxVerboseLevel->addItem( tr("Detailed"), 2 );
	m_ui->comboBoxVerboseLevel->addItem( tr("Very Detailed"), 3 );

	m_ui->lineEditDuration->setup(m_ui->comboBoxUnitDuration, IBK::Unit("s"),
								  0, std::numeric_limits<double>::max(), tr("Duration of the simulation.") );

	m_ui->lineEditNumThreads->setup(1,64,tr("Number of parallel threads, should be less or equal to the number of physical CPU cores."), true, true);
	m_ui->lineEditNumThreads->setAcceptOnlyInteger(true);
	m_ui->lineEditNumThreads->setEmptyAllowed(true, tr("auto (using OMP_NUM_THREADS if set)","as in automatic"));
	bool ok;
	int numThreads = SVSettings::instance().m_propertyMap[SVSettings::PT_NumParallelThreads].toInt(&ok);
	if (ok) {
		if (numThreads == 0)
			m_ui->lineEditNumThreads->setText(""); // 0 = auto (empty input field)
		else
			m_ui->lineEditNumThreads->setValue(numThreads);
	}


	// for now set the defaults states hard-coded, later this should be read from stored settings
	m_ui->comboBoxVerboseLevel->setCurrentIndex(1);
#ifdef WIN32
	m_ui->checkBoxCloseConsoleWindow->setChecked(true);
	m_ui->labelTerminalEmulator->setVisible(false);
	m_ui->comboBoxTermEmulator->setVisible(false);
#elif defined(Q_OS_LINUX)
	m_ui->checkBoxCloseConsoleWindow->setVisible(false);
#else
	// mac has neither option
	m_ui->checkBoxCloseConsoleWindow->setVisible(false);
	m_ui->labelTerminalEmulator->setVisible(false);
	m_ui->comboBoxTermEmulator->setVisible(false);
#endif
	m_ui->comboBoxTermEmulator->blockSignals(true);
	m_ui->comboBoxTermEmulator->setCurrentIndex(SVSettings::instance().m_terminalEmulator);
	m_ui->comboBoxTermEmulator->blockSignals(false);


	{
		m_simulationPerformanceOptions = new SVSimulationPerformanceOptions(this, m_solverParams);
		QHBoxLayout * h = new QHBoxLayout;
		h->addWidget(m_simulationPerformanceOptions);
		m_ui->tabPerformanceOptions->setLayout(h);
	}
	{
		m_simulationLocationOptions = new SVSimulationLocationOptions(this, m_location);
		QHBoxLayout * h = new QHBoxLayout;
		h->addWidget(m_simulationLocationOptions);
		m_ui->tabClimate->setLayout(h);
	}
	{
		m_simulationOutputOptions = new SVSimulationOutputOptions(this, m_outputs);
		QHBoxLayout * h = new QHBoxLayout;
		h->addWidget(m_simulationOutputOptions);
		m_ui->tabOutputs->setLayout(h);
	}
	{
		m_simulationModelOptions = new SVSimulationModelOptions(this, m_simParams, m_location);
		QHBoxLayout * h = new QHBoxLayout;
		h->addWidget(m_simulationModelOptions);
		m_ui->tabSimOptions->setLayout(h);
	}
}


SVSimulationStartNandrad::~SVSimulationStartNandrad() {
	delete m_ui;
}


int SVSimulationStartNandrad::edit() {

	m_solverExecutable = QFileInfo(SVSettings::instance().m_installDir + "/NandradSolver").filePath();
#ifdef WIN32
	m_solverExecutable += ".exe";
#endif // WIN32

	QString nandradProjectFilePath = QFileInfo(SVProjectHandler::instance().projectFile()).completeBaseName() + ".nandrad";
	m_nandradProjectFilePath = QFileInfo(SVProjectHandler::instance().projectFile()).dir().filePath(nandradProjectFilePath);

	// store current project settings
	m_solverParams = project().m_solverParameter;
	m_location = project().m_location;
	m_outputs = project().m_outputs;
	m_simParams = project().m_simulationParameter;


	if (m_simParams == NANDRAD::SimulationParameter()) {
		m_simParams.m_solarLoadsDistributionModel.m_distributionType = NANDRAD::SolarLoadsDistributionModel::SWR_AreaWeighted;
		NANDRAD::KeywordList::setParameter(m_simParams.m_solarLoadsDistributionModel.m_para,
										   "SolarLoadsDistributionModel::para_t",
										   NANDRAD::SolarLoadsDistributionModel::P_RadiationLoadFractionZone, 50);
	}
	// initialize simulation parameters with meaningful defaults and fix possibly wrong values
	// in project (wherever they come from)
	if (m_simParams.m_intPara[NANDRAD::SimulationParameter::IP_StartYear].name.empty() ||
		m_simParams.m_intPara[NANDRAD::SimulationParameter::IP_StartYear].value < 0)
	{
		m_simParams.m_intPara[NANDRAD::SimulationParameter::IP_StartYear].set("StartYear", 2019);
	}

	if (m_simParams.m_interval.m_para[ NANDRAD::Interval::P_Start].name.empty() ||
		m_simParams.m_interval.m_para[ NANDRAD::Interval::P_Start].value < 0 ||
		m_simParams.m_interval.m_para[ NANDRAD::Interval::P_Start].value > 365*24*3600)
	{
		m_simParams.m_interval.m_para[ NANDRAD::Interval::P_Start].set("Start", 0, IBK::Unit("d"));
	}

	if (m_simParams.m_interval.m_para[ NANDRAD::Interval::P_End].name.empty() ||
		m_simParams.m_interval.m_para[ NANDRAD::Interval::P_End].value < 0)
	{
		m_simParams.m_interval.m_para[ NANDRAD::Interval::P_End].set("End", 1, IBK::Unit("a"));
	}
	if (m_simParams.m_para[NANDRAD::SimulationParameter::P_InitialTemperature].name.empty() ||
		m_simParams.m_para[NANDRAD::SimulationParameter::P_InitialTemperature].IO_unit.base_id() != IBK_UNIT_ID_SECONDS)
	{
		NANDRAD::KeywordList::setParameter(m_simParams.m_para,
										   "SimulationParameter::para_t",
										   NANDRAD::SimulationParameter::P_InitialTemperature, 20);
	}

	// create default output settings, if nothing has been defined, yet
	if (m_outputs == VICUS::Outputs()) {
		m_outputs.m_flags[VICUS::Outputs::F_CreateDefaultZoneOutputs].set("CreateDefaultZoneOutputs", true);
		NANDRAD::OutputGrid og;
		og.m_name = tr("Hourly values").toStdString();
		NANDRAD::Interval iv;
		NANDRAD::KeywordList::setParameter(iv.m_para, "Interval::para_t", NANDRAD::Interval::P_Start, 0);
//		NANDRAD::KeywordList::setParameter(iv.m_para, "Interval::para_t", NANDRAD::Interval::P_End, 0);
		NANDRAD::KeywordList::setParameter(iv.m_para, "Interval::para_t", NANDRAD::Interval::P_StepSize, 1);
		og.m_intervals.push_back(iv);
		m_outputs.m_grids.push_back(og);
	}

	m_simulationPerformanceOptions->updateUi();
	m_simulationLocationOptions->updateUi();
	m_simulationOutputOptions->updateUi();
	m_simulationModelOptions->updateUi();

	updateTimeFrameEdits();
	updateCmdLine();

	return exec();
}


void SVSimulationStartNandrad::on_pushButtonClose_clicked() {
	// store data in project and close dialog
	storeInput();
	close();
}


void SVSimulationStartNandrad::on_pushButtonRun_clicked() {
	startSimulation(false);

	// TODO : should we keep the dialog open if simulation crashed?
	storeInput();
	close(); // finally close dialog
}


void SVSimulationStartNandrad::on_checkBoxCloseConsoleWindow_toggled(bool /*checked*/) {
	updateCmdLine();
}


void SVSimulationStartNandrad::on_checkBoxStepStats_toggled(bool /*checked*/) {
	updateCmdLine();
}


void SVSimulationStartNandrad::on_pushButtonShowScreenLog_clicked() {
	// compose path to log file
	// compose log file name
	QString logfile = QFileInfo(m_nandradProjectFilePath).completeBaseName() + "/log/screenlog.txt";
	logfile = QFileInfo(m_nandradProjectFilePath).dir().absoluteFilePath(logfile);
	SVLogFileDialog dlg(this);
	dlg.setLogFile(logfile, m_nandradProjectFilePath, false);
	dlg.exec();
}


void SVSimulationStartNandrad::on_lineEditStartDate_editingFinished() {
	IBK::Time startTime = IBK::Time::fromDateTimeFormat(m_ui->lineEditStartDate->text().toStdString());

	// update date time
	m_simParams.m_intPara[NANDRAD::SimulationParameter::IP_StartYear].set("StartYear", startTime.year());
	m_simParams.m_interval.m_para[NANDRAD::Interval::P_Start].set("Start", startTime.secondsOfYear(), IBK::Unit("s"));
	updateTimeFrameEdits();
}


void SVSimulationStartNandrad::on_lineEditEndDate_editingFinished() {
	IBK::Time endTime = IBK::Time::fromDateTimeFormat(m_ui->lineEditEndDate->text().toStdString());

	// compose start time (startYear and offset are given and well defined, we ensure that)
	int startYear = m_simParams.m_intPara[NANDRAD::SimulationParameter::IP_StartYear].value;
	double offset = m_simParams.m_interval.m_para[NANDRAD::Interval::P_Start].value;
	IBK::Time startTime(startYear, offset);

	// compute difference between dates
	IBK::Time diff = endTime - startTime; // Might be negative!
	if (!diff.isValid()) {
		m_ui->lineEditDuration->setValue(0); // set zero duration to indicate that something is wrong!
		return;
	}

	// end date is the offset from start, so we first need the start date
	m_simParams.m_interval.m_para[NANDRAD::Interval::P_End].set("End", diff.secondsOfYear(), IBK::Unit("s"));
	m_simParams.m_interval.m_para[NANDRAD::Interval::P_End].IO_unit = m_ui->lineEditDuration->currentUnit();

	updateTimeFrameEdits();
}


void SVSimulationStartNandrad::on_lineEditDuration_editingFinishedSuccessfully() {
	// we always update the end time and let the end time signal do the undo action stuff
	IBK::Parameter durPara = m_ui->lineEditDuration->toParameter("Duration");
	if (durPara.name.empty())
		return; // invalid input in parameter edit

	if (durPara.value <= 0)
		return; // invalid input in parameter edit

	int startYear = m_simParams.m_intPara[NANDRAD::SimulationParameter::IP_StartYear].value;
	double offset = m_simParams.m_interval.m_para[NANDRAD::Interval::P_Start].value;
	IBK::Time startTime(startYear, offset);

	// add duration
	startTime += durPara.value;
	m_simParams.m_interval.m_para[NANDRAD::Interval::P_End].set("End", startTime.secondsOfYear(), IBK::Unit("s"));
	// set duration unit in parameter - this will be used to select matching unit in combo box
	m_simParams.m_interval.m_para[NANDRAD::Interval::P_End].IO_unit = durPara.IO_unit;
	updateTimeFrameEdits();
}


void SVSimulationStartNandrad::updateCmdLine() {
	m_cmdArgs.clear();

	if (m_ui->checkBoxStepStats->isChecked())
		m_cmdArgs.push_back("--step-stats");
	if (m_ui->checkBoxCloseConsoleWindow->isChecked())
		m_cmdArgs.push_back("-x");

	m_ui->lineEditCmdLine->setText("\"" + m_solverExecutable + "\" " + m_cmdArgs.join(" ") + "\"" + m_nandradProjectFilePath + "\"");
	m_ui->lineEditCmdLine->setCursorPosition( m_ui->lineEditCmdLine->text().length() );
}

struct ComponentInstance {
	ComponentInstance() {}
	ComponentInstance(unsigned int componentID, unsigned int sideARoomID, unsigned int sideBRoomID) :
		m_componentID(componentID),
		m_sideARoomID(sideARoomID),
		m_sideBRoomID(sideBRoomID)
	{
	}

	unsigned int m_componentID = 0;
	unsigned int m_sideARoomID = 0; // ambient by default
	unsigned int m_sideBRoomID = 0; // ambient by default
};

/*! This exception class collects information about errors that occurred during transformation of data.
	The data in this class can be used to select the problematic geometry so that errors/missing data can be fixed quickly.
*/
class ConversionError : public IBK::Exception {
public:
	/*! TODO: */
	ConversionError(const std::string & errmsg) : IBK::Exception(errmsg, "ConversionError") {}
	ConversionError(const IBK::FormatString & errmsg) : IBK::Exception(errmsg, "ConversionError") {}
};

bool SVSimulationStartNandrad::generateNandradProject(NANDRAD::Project & p) {

	// simulation settings
	p.m_simulationParameter = m_simParams;

	// solver parameters
	p.m_solverParameter = m_solverParams;

	// location settings
	p.m_location = m_location;
	// do we have a climate path?
	if (!m_location.m_climateFileName.isValid()) {
		m_ui->tabWidget->setCurrentWidget(m_ui->tabClimate);
		QMessageBox::critical(this, tr("Starting NANDRAD simulation"), tr("Please select a climate data file!"));
		return false;
	}

	// geometry

	// we process all zones and buildings and create NANDRAD project data
	// we also check that all referenced database properties are available and transfer them accordingly

	// first we build up a directory of all component-instances and their left/right neighbors
//	// TODO : this directory might be part of VICUS::Project, if it is needed elsewhere in the user interface
//	std::map<unsigned int, ComponentInstance> componentList;
//	for (const VICUS::Building & b : project().m_buildings) {
//		for (const VICUS::BuildingLevel & bl : b.m_buildingLevels) {
//			for (const VICUS::Room & r : bl.m_rooms) {
//				for (const VICUS::Surface & s : r.m_surfaces) {
//					if (s.m_componentId == VICUS::INVALID_ID)
//						throw ConversionError(IBK::FormatString("Missing component ID in surface #%1.").arg(s.m_id));
//					ComponentInstance & ci = componentList[s.m_componentId];

//				}
//			}
//		}
//	}

	// this set collects all component instances that are actually used/referenced by zone surfaces
	// for now, unassociated components are ignored
	std::set<unsigned int> usedComponentInstances;

	for (const VICUS::Building & b : project().m_buildings) {
		for (const VICUS::BuildingLevel & bl : b.m_buildingLevels) {
			for (const VICUS::Room & r : bl.m_rooms) {
				// first create a NANDRAD zone for the room
				NANDRAD::Zone z;
				z.m_id = r.m_id;
				z.m_displayName = r.m_displayName.toStdString();
				// Note: in the code below we expect the parameter's base units to be the same as the default unit for the
				//       populated parameters

				// TODO : what if we do not have an area or a zone volume, yet?
				NANDRAD::KeywordList::setParameter(z.m_para, "Zone::para_t", NANDRAD::Zone::P_Area, r.m_para[VICUS::Room::P_Area].value);
				NANDRAD::KeywordList::setParameter(z.m_para, "Zone::para_t", NANDRAD::Zone::P_Volume, r.m_para[VICUS::Room::P_Volume].value);

				// for now, zones are always active
				z.m_type = NANDRAD::Zone::ZT_Active;

				// now process all surfaces
				for (const VICUS::Surface & s : r.m_surfaces) {
					// each surface can be either a construction to the outside, to a fixed zone or to a different zone
					// the latter is only recognized, if we search through all zones and check their association with a surface.

				}
			}
		}
	}


	// outputs



	return true;
}


void SVSimulationStartNandrad::storeInput() {

	/// TODO : create an undo action for modification of the project

}


void SVSimulationStartNandrad::updateTimeFrameEdits() {

	m_ui->lineEditStartDate->blockSignals(true);
	m_ui->lineEditEndDate->blockSignals(true);
	m_ui->lineEditDuration->blockSignals(true);

	// Note: we can be sure that all the parameters are set, though possibly to invalid values

	int startYear = m_simParams.m_intPara[NANDRAD::SimulationParameter::IP_StartYear].value;
	// fall-back to zero, if not specified
	double startOffset = m_simParams.m_interval.m_para[ NANDRAD::Interval::P_Start].value;

	IBK::Time t(startYear, startOffset);
	m_ui->lineEditStartDate->setText( QString::fromStdString(t.toDateTimeFormat()) );

	double endTime = m_simParams.m_interval.m_para[ NANDRAD::Interval::P_End].value;
	if (m_simParams.m_interval.m_para[ NANDRAD::Interval::P_End].name.empty())
		endTime = startOffset + 365*24*3600; // fallback to 1 year
	double simDuration = endTime - startOffset;
	t += simDuration;

	m_ui->lineEditEndDate->setText( QString::fromStdString(t.toDateTimeFormat()) );

	IBK::Parameter durationPara;
	// use unit from end
	durationPara = IBK::Parameter("Duration", 0, m_simParams.m_interval.m_para[ NANDRAD::Interval::P_End].IO_unit);
	durationPara.value = simDuration; // set value in seconds
	m_ui->lineEditDuration->setFromParameter(durationPara);

	m_ui->lineEditStartDate->blockSignals(false);
	m_ui->lineEditEndDate->blockSignals(false);
	m_ui->lineEditDuration->blockSignals(false);
}


void SVSimulationStartNandrad::startSimulation(bool testInit) {
	// compose NANDRAD project file and start simulation

	// generate NANDRAD project
	NANDRAD::Project p;

	p.m_location = m_location;
	p.m_solverParameter = m_solverParams;
	p.m_simulationParameter = m_simParams;

	if (!generateNandradProject(p))
		return;

	// save project
	p.writeXML(IBK::Path(m_nandradProjectFilePath.toStdString()));
	/// TODO : check if project file was correctly written

	QString resultPath = QFileInfo(SVProjectHandler::instance().projectFile()).completeBaseName();
	resultPath = QFileInfo(SVProjectHandler::instance().projectFile()).dir().filePath(resultPath);
	IBK::Path resultDir(resultPath.toStdString());

	bool cleanDir = false;
	QStringList commandLineArgs = m_cmdArgs;
	if (testInit) {
		commandLineArgs.append("--test-init");
	}
	else {
		SVSimulationRunRequestDialog::SimulationStartType startType = SVSimulationRunRequestDialog::Normal;
		// check if result directory exists and if yes, ask user about overwriting
		if (resultDir.exists()) {
			if (!resultDir.isDirectory()) {
				QMessageBox::critical(this, tr("Solver error"),
									  tr("There is already a file with the name of the output "
										 "directory to be created '%1'. Please remove this file "
										 "or save the project with a new name!").arg(resultPath));
				return;
			}
			// ask user for confirmation
			if (m_simulationRunRequestDialog == nullptr)
				m_simulationRunRequestDialog = new SVSimulationRunRequestDialog(this);
			startType = m_simulationRunRequestDialog->askForOption();
			// if user aborted dialog, do nothing
			if (startType == SVSimulationRunRequestDialog::DoNotRun)
				return;
			// only clean directory when user selected normal
			if (startType == SVSimulationRunRequestDialog::Normal)
				cleanDir = true;
		}

		// add command line option if needed
		if (startType == SVSimulationRunRequestDialog::Continue)
			commandLineArgs.append("--restart");
	}
	// clean result directory if requested
	if (cleanDir) {
		// We only delete a subdirectory with correct subdirectory structure. This
		// generally prevents accidental deleting of directories.
		IBK::Path resFolder = resultDir / "results";
		IBK::Path logFolder = resultDir / "log";
		if (resFolder.exists() && logFolder.exists()) {
			if (!IBK::Path::remove(resultDir)) {
				QMessageBox::critical(this, tr("Solver error"),
									  tr("Cannot remove result directory '%1', maybe files are still being used?").arg(resultPath) );
				return;
			}
		}
	}

	// delete working directory if requested
	// launch solver - run option is only needed for linux, and otherwise it will always be -1
	SVSettings::TerminalEmulators runOption = (SVSettings::TerminalEmulators)m_ui->comboBoxTermEmulator->currentIndex();
	bool success = SVSettings::startProcess(m_solverExecutable, commandLineArgs, m_nandradProjectFilePath, runOption);
	if (!success) {
		QMessageBox::critical(this, QString(), tr("Could not run solver '%1'").arg(m_solverExecutable));
		return;
	}
}


void SVSimulationStartNandrad::on_comboBoxTermEmulator_currentIndexChanged(int index) {
	SVSettings::instance().m_terminalEmulator = (SVSettings::TerminalEmulators)(index);
}



void SVSimulationStartNandrad::on_pushButtonTestInit_clicked() {
	startSimulation(true);
}
