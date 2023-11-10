#include "SVSubNetworkEditDialog.h"
#include "ui_SVSubNetworkEditDialog.h"

#include "SVBMZoomMeshGraphicsView.h"
#include "SVBMSceneManager.h"
#include "SVSubNetworkEditDialogTable.h"
#include "SVDBSubNetworkEditWidget.h"
#include "SVNetworkControllerEditDialog.h"
#include "SVDatabase.h"
#include "SVSettings.h"
#include "SVProjectHandler.h"

#include <queue>
#include <QScreen>
#include <QKeyEvent>
#include <QDir>

#include <NANDRAD_HydraulicNetworkControlElement.h>
#include <NANDRAD_HydraulicNetworkComponent.h>

#include <IBK_MultiLanguageString.h>

#include <QtExt_Directories.h>

#include <VICUS_BMNetwork.h>
#include <VICUS_BMBlock.h>
#include <VICUS_BMGlobals.h>
#include <VICUS_BMGlobals.h>
#include <VICUS_SubNetwork.h>
#include <VICUS_NetworkComponent.h>
#include <VICUS_NetworkElement.h>
#include <VICUS_KeywordListQt.h>
#include <VICUS_utilities.h>
#include <VICUS_Database.h>


const int MINIMUMDIALOGWIDTH = 1500;
const int MINIMUMDIALOGHEIGHT = 980;

SVSubNetworkEditDialog::SVSubNetworkEditDialog(QWidget *parent, VICUS::SubNetwork * subNetwork, SVDatabase * db) :
	QDialog(parent),
	m_ui(new Ui::SVSubNetworkEditDialog)
{
	m_ui->setupUi(this);

	m_db = db;

	// resize window
	QScreen *screen = QGuiApplication::primaryScreen();
	Q_ASSERT(screen!=nullptr);
	QRect rect = screen->geometry();
	int width = int(rect.width()* 0.95);
	int height = int(0.8*rect.height());

	if(width < MINIMUMDIALOGWIDTH){
		width = MINIMUMDIALOGWIDTH;
	}
	if(height < MINIMUMDIALOGHEIGHT){
		height = MINIMUMDIALOGHEIGHT;
	}

	resize(width, height);
	m_ui->scrollArea->setMaximumHeight(height-20);


	// populate tool box
	m_ui->tbox->blockSignals(true);
	m_ui->tbox->m_enableOpenMultiplePages = true;
	m_ui->tbox->layout()->setMargin(0);

	updateToolBoxPages();

	m_ui->tbox->addPage(tr("Pipes"), m_tables[0]);
	m_ui->tbox->addPage(tr("Pumps"), m_tables[1]);
	m_ui->tbox->addPage(tr("Heatpumps"), m_tables[2]);
	m_ui->tbox->addPage(tr("Other"), m_tables[3]);
	m_ui->tbox->blockSignals(false);
	m_ui->tbox->setCurrentIndex(0);
	m_ui->viewWidget->setDragMode(QGraphicsView::ScrollHandDrag);
	m_ui->viewWidget->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	m_ui->viewWidget->setResolution(1);

	m_ui->controllerLineEdit->setDisabled(true);

	m_sceneManager = qobject_cast<SVBMSceneManager*>(m_ui->viewWidget->scene());
	setupSubNetwork(subNetwork);

	connect(m_sceneManager, &SVBMSceneManager::newBlockSelected, this, &SVSubNetworkEditDialog::blockSelectedEvent);
	connect(m_sceneManager, &SVBMSceneManager::newConnectorSelected, this, &SVSubNetworkEditDialog::connectorSelectedEvent);
	connect(m_sceneManager, &SVBMSceneManager::selectionCleared, this, &SVSubNetworkEditDialog::selectionClearedEvent);
	connect(m_sceneManager, &SVBMSceneManager::newBlockAdded, this, &SVSubNetworkEditDialog::on_newBlockAdded);
	connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &SVSubNetworkEditDialog::on_buttonBox_accepted);
	connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &SVSubNetworkEditDialog::on_buttonBox_rejected);
	connect(m_ui->nameLineEdit, &QLineEdit::textChanged, this, &SVSubNetworkEditDialog::on_NameTextChanged);
	connect(&SVProjectHandler::instance(), &SVProjectHandler::updateSubnetworkScreenshots, this, &SVSubNetworkEditDialog::on_projectSaved);
}

SVSubNetworkEditDialog::~SVSubNetworkEditDialog()
{
	delete m_ui;
}

void SVSubNetworkEditDialog::setupSubNetwork(VICUS::SubNetwork *subNetwork)
{
	m_subNetwork = subNetwork;
	if(m_subNetwork->m_elements.size() != 0 && m_subNetwork->m_components.size() == 0){
		convertSubnetwork();
	}
	updateNetwork();
	m_ui->networkComponentEditWidget->setFixedWidth(491);
}

void SVSubNetworkEditDialog::open()
{
	QDialog::open();

	QList<int> ratio;
	ratio << 300 << m_ui->splitter->width()-300;
	m_ui->splitter->setSizes(ratio);

	int viewWidth = m_ui->viewWidget->size().width();
	int viewHeight = m_ui->viewWidget->size().height();
	double aspectRatio = double(viewWidth)/double(viewHeight);
	int sceneWidth = 1151;
	int sceneHeight = int(sceneWidth/aspectRatio);
	m_sceneManager->setSceneRect(0,450-sceneHeight/2,sceneWidth,450+sceneHeight/2);
	m_ui->viewWidget->fitInView(m_sceneManager->sceneRect(), Qt::KeepAspectRatio);
	selectionClearedEvent();
	m_sceneManager->update();
}

void SVSubNetworkEditDialog::resize(int w, int h)
{
	QDialog::resize(QSize(w,h));
	m_ui->scrollArea->setMaximumHeight(h-20);
}

void SVSubNetworkEditDialog::updateToolBoxPages(){

	if(m_tables.size() == 0){
		for(unsigned int i = 0; i < VICUS::NetworkComponent::ComponentCategory::NUM_CC; i++){
			m_tables.push_back(new SVSubNetworkEditDialogTable(this));
			connect(m_tables[i], &SVSubNetworkEditDialogTable::itemSelectionChanged, this, &SVSubNetworkEditDialog::on_componentSelected);
		}
	} else {
		for (SVSubNetworkEditDialogTable *table : m_tables){
			table->clearSelection();
			table->clear();
		}
	}

	for(int i = 0; i < VICUS::NetworkComponent::ModelType::NUM_MT; ++i) {
		VICUS::NetworkComponent::ModelType modelType = VICUS::NetworkComponent::ModelType(i);
		VICUS::NetworkComponent::ComponentCategory category = VICUS::NetworkComponent::componentCategoryFromModelType(modelType);
		switch(category){
		case VICUS::NetworkComponent::CC_Pipes: {
			m_tables[category]->addElement(modelType);
			for (auto it = m_db->m_networkComponents.begin(); it != m_db->m_networkComponents.end(); ++it) {
				if(it->second.m_modelType == modelType){
					m_tables[category]->addElement(it->second);
				}
			}
		} break;
		case VICUS::NetworkComponent::CC_Pumps: {
			m_tables[category]->addElement(modelType);
			for (auto it = m_db->m_networkComponents.begin(); it != m_db->m_networkComponents.end(); ++it) {
				if(it->second.m_modelType == modelType){
					m_tables[category]->addElement(it->second);
				}
			}
		} break;
		case VICUS::NetworkComponent::CC_Heatpumps: {
			m_tables[category]->addElement(modelType);
			for (auto it = m_db->m_networkComponents.begin(); it != m_db->m_networkComponents.end(); ++it) {
				if(it->second.m_modelType == modelType){
					m_tables[category]->addElement(it->second);
				}
			}
		} break;
		case VICUS::NetworkComponent::CC_Other: {
			m_tables[category]->addElement(modelType);
			for (auto it = m_db->m_networkComponents.begin(); it != m_db->m_networkComponents.end(); ++it) {
				if(it->second.m_modelType == modelType){
					m_tables[category]->addElement(it->second);
				}
			}
		} break;
		default:
			qDebug() << "no entry for VICUS::NetworkComponent::ModelType " << modelType;
		}
	}

	m_ui->tbox->update();
}


void SVSubNetworkEditDialog::updateNetwork() {
	FUNCID(SVSubNetworkEditDialog::updateNetwork);
	m_networkComponents = m_subNetwork->m_components;
	m_networkControllers = m_subNetwork->m_controllers;

	// if BMNetwork empty,create new network, fill it with NetworkElements from SubNetwork
	if(m_subNetwork->m_graphicalNetwork.m_blocks.size() == 0){
		m_sceneManager->updateNetwork(m_subNetwork->m_graphicalNetwork);

		VICUS::BMBlock bentry, bexit;
		bentry.m_name = VICUS::SUBNETWORK_INLET_NAME;
		bentry.m_mode = VICUS::BMBlockType::GlobalInlet;
		bentry.m_size = QSizeF(VICUS::ENTRANCEEXITBLOCK_WIDTH,VICUS::ENTRANCEEXITBLOCK_HEIGHT);
		bentry.m_pos = QPointF(0,400);

		bexit.m_name = VICUS::SUBNETWORK_OUTLET_NAME;
		bexit.m_mode = VICUS::BMBlockType::GlobalOutlet;
		bexit.m_size = QSizeF(VICUS::ENTRANCEEXITBLOCK_WIDTH,VICUS::ENTRANCEEXITBLOCK_HEIGHT);
		bexit.m_pos = QPointF(1102,400);

		VICUS::BMSocket outlet, inlet;
		inlet.m_name = VICUS::INLET_NAME;
		inlet.m_isInlet = true;
		inlet.m_id = VICUS::EXIT_ID;
		inlet.m_orientation = Qt::Horizontal;
		inlet.m_pos = QPointF(0,VICUS::ENTRANCEEXITBLOCK_HEIGHT / 2);
		bexit.m_sockets.append(inlet);

		outlet.m_name = VICUS::OUTLET_NAME;
		outlet.m_isInlet = false;
		outlet.m_id = VICUS::ENTRANCE_ID;
		outlet.m_orientation = Qt::Horizontal;
		outlet.m_pos = QPointF(VICUS::ENTRANCEEXITBLOCK_WIDTH,VICUS::ENTRANCEEXITBLOCK_HEIGHT / 2);
		bentry.m_sockets.append(outlet);

		m_sceneManager->addBlock(bentry);
		m_sceneManager->addBlock(bexit);

		int counter = 0;
		int numberOfBlocks = m_subNetwork->m_elements.size() + 1;
		int distancePerBlock = (1102 - VICUS::ENTRANCEEXITBLOCK_WIDTH) / numberOfBlocks - VICUS::CONNECTORBLOCK_WIDTH;

		VICUS::BMBlock previous = bentry;

		for(VICUS::NetworkElement element : m_subNetwork->m_elements){
			VICUS::BMBlock block = VICUS::BMBlock(QString::number(counter));
			VICUS::BMSocket outlet, inlet;
			block.m_componentId = element.m_componentId;
			block.m_controllerID = element.m_controlElementId;

			VICUS::NetworkComponent::ModelType type = m_networkComponents.back().m_modelType;

			block.m_displayName = element.m_displayName;
			block.m_mode = VICUS::BMBlockType::NetworkComponentBlock;
			block.m_size = QSizeF(VICUS::BLOCK_WIDTH, VICUS::BLOCK_HEIGHT);
			QPoint point(VICUS::ENTRANCEEXITBLOCK_WIDTH + (counter + 1) * distancePerBlock + counter * VICUS::CONNECTORBLOCK_WIDTH, 400);
			block.m_pos = point;
			block.m_properties["ShowPixmap"] = true;
			block.m_properties["Pixmap"] = QPixmap(VICUS::NetworkComponent::iconFileFromModelType(type)).scaled(256,256);

			inlet.m_name = VICUS::INLET_NAME;
			inlet.m_isInlet = true;
			inlet.m_id = element.m_inletNodeId;
			inlet.m_orientation = Qt::Horizontal;
			inlet.m_pos = QPointF(0, VICUS::BLOCK_HEIGHT / 2);
			block.m_sockets.append(inlet);

			outlet.m_name = VICUS::OUTLET_NAME;
			outlet.m_isInlet = false;
			outlet.m_id = element.m_outletNodeId;
			outlet.m_orientation = Qt::Horizontal;
			outlet.m_pos = QPointF(VICUS::BLOCK_WIDTH, VICUS::BLOCK_HEIGHT / 2);
			block.m_sockets.append(outlet);

			m_sceneManager->addBlock(block);

			if(element.m_inletNodeId == VICUS::ENTRANCE_ID){
				m_sceneManager->createConnection(&previous, &block, &previous.m_sockets[0], &block.m_sockets[0]);
			}
			else{
				m_sceneManager->createConnection(&previous, &block, &previous.m_sockets[1], &block.m_sockets[0]);
			}
			if(element.m_outletNodeId == VICUS::EXIT_ID){
				m_sceneManager->createConnection(&block, &bexit, &block.m_sockets[1], &bexit.m_sockets[0]);
			}
			previous = block;

			counter++;
		}
	}
	 else {
		m_sceneManager->updateNetwork(m_subNetwork->m_graphicalNetwork);
	}

	// add Controller displayName to BlockItem
	for(auto& block : m_sceneManager->network().m_blocks){
		if(block.m_controllerID != VICUS::INVALID_ID){
			const VICUS::NetworkController *ctr = VICUS::element(m_networkControllers, block.m_controllerID);
			m_sceneManager->setControllerID(&block, block.m_controllerID, VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", ctr->m_controlledProperty));
		}
	}

}


bool SVSubNetworkEditDialog::checkAcceptedNetwork()
{
	/* checks if every block has at least one inlet and one outlet connection, if not,
	throw warning and decline to close the window */
	for(const VICUS::BMBlock &block : m_sceneManager->network().m_blocks){
		if (block.m_mode == VICUS::BMBlockType::NetworkComponentBlock){
			if(!(m_sceneManager->isConnectedSocket(&block, &(block.m_sockets[0])) && m_sceneManager->isConnectedSocket(&block, &(block.m_sockets[1])))){
				QMessageBox::warning(this, tr("Invalid Network"), tr("Not all blocks are properly connected. Invalid networks can not be saved."));
				return false;
			}
		}
	}

	// Existing block connectivity setup
	std::map<unsigned int, std::vector<unsigned int>> connections;
	std::map<unsigned int, std::vector<unsigned int>> reverseConnections;
	for (auto& block : m_sceneManager->network().m_blocks){
		if (block.m_mode == VICUS::BMBlockType::GlobalInlet || block.m_mode == VICUS::BMBlockType::GlobalOutlet || block.m_mode == VICUS::BMBlockType::ConnectorBlock)
			continue;
		unsigned int inletID = block.m_sockets[0].m_id;
		unsigned int outletID = block.m_sockets[1].m_id;
		connections[inletID].push_back(outletID);
		reverseConnections[outletID].push_back(inletID);  // Reverse mapping for outlet traversal
	}

	// Traversal from global inlet
	std::set<unsigned int> visitedFromInlet;
	std::queue<unsigned int> queue;
	queue.push(VICUS::ENTRANCE_ID);
	while(!queue.empty()){
		unsigned int node = queue.front();
		queue.pop();
		visitedFromInlet.insert(node);

		if(connections.find(node) != connections.end()){
			for(auto& outlet : connections[node]){
				if(visitedFromInlet.find(outlet) == visitedFromInlet.end()){
					queue.push(outlet);
				}
			}
		}
	}

	// Traversal from global outlet using reversed graph
	std::set<unsigned int> visitedFromOutlet;
	queue.push(VICUS::EXIT_ID);
	while(!queue.empty()){
		unsigned int node = queue.front();
		queue.pop();
		visitedFromOutlet.insert(node);

		if(reverseConnections.find(node) != reverseConnections.end()){
			for(auto& inlet : reverseConnections[node]){
				if(visitedFromOutlet.find(inlet) == visitedFromOutlet.end()){
					queue.push(inlet);
				}
			}
		}
	}

	// Check for unreachable blocks
	bool allReachable = true;
	for(auto& block : m_sceneManager->network().m_blocks){
		if (block.m_mode == VICUS::BMBlockType::GlobalInlet || block.m_mode == VICUS::BMBlockType::GlobalOutlet|| block.m_mode == VICUS::BMBlockType::ConnectorBlock)
			continue;
		unsigned int inletID = block.m_sockets[0].m_id;
		unsigned int outletID = block.m_sockets[1].m_id;

		if (visitedFromInlet.find(inletID) == visitedFromInlet.end() || visitedFromOutlet.find(outletID) == visitedFromOutlet.end()) {
			// Block is not fully reachable
			allReachable = false;
			break;
		}
	}

	if(!allReachable) {
		QMessageBox::warning(this, tr("Invalid Network"), tr("Not all Blocks are reachable from globalInlet and globalOutlet. Invalid networks can not be saved."));
		return false;
	}

	return true;
}

void SVSubNetworkEditDialog::on_buttonBox_accepted()
{
	// if the network is invalid we do nothing
	if (!checkAcceptedNetwork())
		return;

	m_sceneManager->clearSelection();

	// create splash screen
	QPixmap p = m_sceneManager->generatePixmap(QSize(815, 480));
	QString thumbNailPath = QtExt::Directories::userDataDir()  + "/thumbs";
	if (!QDir(thumbNailPath).exists())
		QDir().mkdir(thumbNailPath);

	QString thumbName = SVProjectHandler::instance().projectFile();
	thumbName.replace("/", "_");
	thumbName.replace("\\", "_");
	thumbName.replace(":", "_");
	QString thumbPath = QtExt::Directories::userDataDir()  + "/thumbs/" + "~SN" + thumbName + QString::number(m_subNetwork->m_id) + ".png";
	p.save(thumbPath);
	qDebug() << "Saved at: " << thumbPath;

	// we transfer the edited network
	m_subNetwork->m_graphicalNetwork = m_sceneManager->network();
	m_subNetwork->m_elements.clear();
	m_subNetwork->m_components.clear();
	m_subNetwork->m_controllers.clear();

	/* Copy all used components */
	std::set<unsigned int> usedComponentIds;
	for (auto& block : m_subNetwork->m_graphicalNetwork.m_blocks) {
		if(block.m_mode == VICUS::BMBlockType::NetworkComponentBlock)
			usedComponentIds.insert(block.m_componentId);
	}
	for (unsigned int id: usedComponentIds) {
		m_subNetwork->m_components.push_back( m_networkComponents[componentIndex(id)] );
	}


	/* Copy all used controllers */
	std::set<unsigned int> usedControllerIds;
	for (auto& block : m_subNetwork->m_graphicalNetwork.m_blocks) {
		if(block.m_mode == VICUS::BMBlockType::NetworkComponentBlock && block.m_controllerID != VICUS::INVALID_ID){
			usedControllerIds.insert(block.m_controllerID);
		}
	}
	for(unsigned int id : usedControllerIds){
		m_subNetwork->m_controllers.push_back( m_networkControllers[controllerIndex(id)] );
	}


	/* creates the appropriate Network Elements for the Subnetwork */
	for (auto& block : m_subNetwork->m_graphicalNetwork.m_blocks){
		if (block.m_mode == VICUS::BMBlockType::NetworkComponentBlock){
			VICUS::NetworkElement networkElement(block.m_name.toUInt(), block.m_sockets[1].m_id, block.m_sockets[0].m_id, block.m_componentId);
			networkElement.m_controlElementId = block.m_controllerID;
			networkElement.m_displayName = block.m_displayName;
			networkElement.m_inletNodeId = block.m_sockets[0].m_id;
			networkElement.m_outletNodeId = block.m_sockets[1].m_id;
			m_subNetwork->m_elements.push_back(networkElement);
		}
	}

	this->close();
}

void SVSubNetworkEditDialog::on_buttonBox_rejected()
{
	this->close();
}

SVBMZoomMeshGraphicsView *SVSubNetworkEditDialog::zoomMeshGraphicsView(){
	return m_ui->viewWidget;
}


void SVSubNetworkEditDialog::blockSelectedEvent()
{

	VICUS::BMBlock  *blockToDisplay = nullptr;
	// retrieve list of selected Blocks
	QList<const VICUS::BMBlock*> blocks = m_sceneManager->selectedBlocks();
	// only fill in right widget if exactly one block is selected
	if (blocks.size() == 1){
		blockToDisplay = const_cast<VICUS::BMBlock*>(blocks.first());
		// if Block is a networkComponentBlock, fill data in the right widget
		if(blockToDisplay->m_mode == VICUS::BMBlockType::NetworkComponentBlock){
			m_ui->stackedWidget->setCurrentIndex(1);
			//activate ControllerGroupBox
			m_ui->controllerGroupBox->setEnabled(true);
			m_ui->controllerLabel->setEnabled(true);
			m_ui->controllerLineEdit->setEnabled(true);
			m_ui->nameLineEdit->setEnabled(true);
			m_ui->nameLabel->setEnabled(true);
			m_ui->removeControllerButton->setEnabled(true);
			m_ui->removeButton->setEnabled(true);
			m_ui->copyBlockButton->setEnabled(true);
			m_ui->addToUserDBButton->setEnabled(true);
			m_ui->nameLineEdit->setText(blockToDisplay->m_displayName);

			std::vector<NANDRAD::HydraulicNetworkControlElement::ControlledProperty> availableCtrProps;
			NANDRAD::HydraulicNetworkComponent::ModelType nandradModelType;
			nandradModelType = VICUS::NetworkComponent::nandradNetworkComponentModelType(m_networkComponents[componentIndex(blockToDisplay->m_componentId)].m_modelType);
			availableCtrProps = NANDRAD::HydraulicNetworkControlElement::availableControlledProperties(nandradModelType);
			if(availableCtrProps.size() > 0){
							m_ui->openControllerWidgetButton->setEnabled(true);
			} else {
							if(blockToDisplay->m_componentId != VICUS::INVALID_ID){
								m_ui->openControllerWidgetButton->setEnabled(true);
							}
							m_ui->openControllerWidgetButton->setEnabled(false);
			}

			if(blockToDisplay->m_controllerID != VICUS::INVALID_ID){
				const VICUS::NetworkController *ctr = VICUS::element(m_networkControllers, blockToDisplay->m_controllerID);
				m_ui->controllerLineEdit->setText(QString::fromLatin1(VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", ctr->m_controlledProperty)));
				m_ui->removeControllerButton->setEnabled(true);
			}
			else{
				m_ui->controllerLineEdit->clear();
				m_ui->removeControllerButton->setEnabled(false);
			}

			// Fill NetworkComponent ComboBox
			VICUS::NetworkComponent* component = &m_networkComponents[componentIndex(blockToDisplay->m_componentId)];
			m_ui->networkComponentEditWidget->updateInput(component);

		// connector block
		} else if (blockToDisplay->m_mode == VICUS::BMBlockType::ConnectorBlock){
			selectionClearedEvent();
			m_ui->removeButton->setEnabled(true);
		// global inlet / outlet
		} else {
			selectionClearedEvent();
		}
	// more than on block selected
	} else {
		selectionClearedEvent();
	}
}


void SVSubNetworkEditDialog::connectorSelectedEvent(const QString & sourceSocketName, const QString & targetSocketName)
{
	// if selection changes from block to connector, selectionClearedEvent is not called, so we do it here
	selectionClearedEvent();
	m_ui->removeButton->setEnabled(true);
}


void SVSubNetworkEditDialog::selectionClearedEvent(){
	m_ui->stackedWidget->setCurrentIndex(0);
	m_ui->networkComponentEditWidget->updateInput(nullptr);
	m_ui->copyBlockButton->setDisabled(true);
	m_ui->addToUserDBButton->setEnabled(false);
	m_ui->removeButton->setDisabled(true);
	m_ui->nameLineEdit->clear();
}


void SVSubNetworkEditDialog::on_NameTextChanged(const QString& text){
	if(m_sceneManager->selectedBlocks().size() == 0)
		return;
	VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().first());
	selectedBlock->m_displayName = text;
	m_sceneManager->update();
}


void SVSubNetworkEditDialog::on_newBlockAdded(VICUS::BMBlock *block, unsigned int componentID) {

	// create new component
	if (componentID == VICUS::INVALID_ID){
		unsigned int newCompID = newComponentID();
		m_networkComponents.push_back(VICUS::NetworkComponent());
		m_networkComponents.back().m_modelType = static_cast<VICUS::NetworkComponent::ModelType>(block->m_componentId);
		QString name = tr("<new %1>").arg( VICUS::KeywordListQt::Keyword("NetworkComponent::ModelType", m_networkComponents.back().m_modelType) );
		m_networkComponents.back().m_displayName.setString(name.toStdString(), IBK::MultiLanguageString::m_language);
		block->m_componentId = newCompID;
		m_networkComponents.back().m_id = newCompID;
	}
	// copy component from DB
	else {
		auto itNetworkComp = m_db->m_networkComponents.begin();
		Q_ASSERT(m_db->m_networkComponents[componentID] != nullptr);
		VICUS::NetworkComponent component =	*m_db->m_networkComponents[componentID];
		component.m_builtIn = false;
		component.m_id = newComponentID();
		block->m_componentId = component.m_id;
		m_networkComponents.push_back(component);
	}
}

void SVSubNetworkEditDialog::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete) {
		if(m_sceneManager->selectedBlocks().size() >= 1){
			m_sceneManager->removeSelectedBlocks();
		}
		else if(m_sceneManager->selectedConnector() != nullptr){
			m_sceneManager->removeConnector(m_sceneManager->selectedConnector());
		}
	}
}

unsigned int SVSubNetworkEditDialog::componentIndex(unsigned int componentID)
{
	for(unsigned int i = 0; i < m_networkComponents.size(); i++){
		if(m_networkComponents[i].m_id == componentID){
			return i;
		}
	}
	// this should never happen
	Q_ASSERT(false);
	return VICUS::INVALID_ID;
}

unsigned int SVSubNetworkEditDialog::newComponentID()
{
	unsigned int id = 0;
	for( unsigned int i = 0; i < m_networkComponents.size(); i++){
		if(m_networkComponents[i].m_id >= id){
			id = m_networkComponents[i].m_id + 1;
		}
	}
	return id;
}

unsigned int SVSubNetworkEditDialog::controllerIndex(unsigned int controllerID)
{
	for( unsigned int i = 0; i < m_networkControllers.size(); i++){
		if(m_networkControllers[i].m_id == controllerID){
			return i;
		}
	}
	// this should never happen
	Q_ASSERT(false);
	return VICUS::INVALID_ID;
}

unsigned int SVSubNetworkEditDialog::newControllerID(){
	unsigned int id = 0;
	for( unsigned int i = 0; i < m_networkControllers.size(); i++){
		if(m_networkControllers[i].m_id >= id){
			id = m_networkControllers[i].m_id + 1;
		}
	}
	return id;
}

void SVSubNetworkEditDialog::convertSubnetwork()
{
	FUNCID(SVSubNetworkEditDialog::convertSubnetwork);
	// iterates over every element and copies component into Subnetwork and create new IDs
	for(VICUS::NetworkElement &element : m_subNetwork->m_elements){
		if(element.m_componentId != VICUS::INVALID_ID){
			VICUS::NetworkComponent *componentPtr = m_db->m_networkComponents[element.m_componentId];
			Q_ASSERT(componentPtr != nullptr);
			VICUS::NetworkComponent component = *componentPtr;
			component.m_id = VICUS::largestUniqueId(m_subNetwork->m_components);
			element.m_componentId = component.m_id;
			m_subNetwork->m_components.push_back(component);
		} else {
			throw IBK::Exception(tr("Invalid networkComponent ID %1  in subnetwork")
									 .arg(element.m_componentId).toStdString(), FUNC_ID);
		}
		// checks if element has a controller, if yes, copy it into subNetwork and create new IDs
		if(element.m_controlElementId != VICUS::INVALID_ID){
			VICUS::NetworkController *controllerPtr = m_db->m_networkControllers[element.m_controlElementId];
			Q_ASSERT(controllerPtr != nullptr);
			VICUS::NetworkController controller = *controllerPtr;
			controller.m_id = VICUS::largestUniqueId(m_subNetwork->m_controllers);
			element.m_controlElementId = controller.m_id;
			m_subNetwork->m_controllers.push_back(controller);
		}
	}
}


void SVSubNetworkEditDialog::on_removeButton_clicked()
{
	if(m_sceneManager->selectedBlocks().size() > 0){
		VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().first());
		if(selectedBlock->m_mode == VICUS::BMBlockType::GlobalInlet || selectedBlock->m_mode == VICUS::BMBlockType::GlobalOutlet)
			return;
		if(selectedBlock->m_mode == VICUS::BMBlockType::ConnectorBlock){
			m_sceneManager->removeConnectorBlock(selectedBlock);
			m_sceneManager->update();
			return;
		}

		if(selectedBlock->m_componentId != VICUS::INVALID_ID)
			m_networkComponents.erase(m_networkComponents.begin() + componentIndex(selectedBlock->m_componentId));
		if(selectedBlock->m_controllerID != VICUS::INVALID_ID)
			m_networkControllers.erase(m_networkControllers.begin() + controllerIndex(selectedBlock->m_controllerID));
		m_sceneManager->removeBlock(selectedBlock);
		m_sceneManager->update();

	} else if(m_sceneManager->selectedConnector()){
		m_sceneManager->removeSelectedConnector();
	}
}


void SVSubNetworkEditDialog::on_openControllerWidgetButton_clicked()
{
	if(m_controllerEditDialog == nullptr){
		m_controllerEditDialog = new SVNetworkControllerEditDialog(this);
	}

	VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().takeFirst());
	Q_ASSERT(selectedBlock != nullptr);
	VICUS::NetworkController controller;
	if(selectedBlock->m_controllerID != VICUS::INVALID_ID){
		controller = m_networkControllers[controllerIndex(selectedBlock->m_controllerID)];
	}
	const VICUS::NetworkComponent &component = m_networkComponents[componentIndex(selectedBlock->m_componentId)];
	m_controllerEditDialog->setup(controller, component.m_modelType);

	m_controllerEditDialog->exec();

	if (m_controllerEditDialog->result() == QDialog::Accepted){
		controller = m_controllerEditDialog->controller();
		if(selectedBlock->m_controllerID == VICUS::INVALID_ID){
			controller.m_id = newControllerID();
			selectedBlock->m_controllerID = controller.m_id;
			m_networkControllers.push_back(controller);
		} else {
			m_networkControllers[controllerIndex(selectedBlock->m_controllerID)] = controller;
		}
		m_ui->controllerLineEdit->setText(VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", static_cast<int>(controller.m_controlledProperty)));
		m_sceneManager->setControllerID(selectedBlock, selectedBlock->m_controllerID, VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", controller.m_controlledProperty));
		m_sceneManager->update();
	}
}


void SVSubNetworkEditDialog::on_removeControllerButton_clicked()
{
	VICUS::BMBlock* selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().first());
	Q_ASSERT(selectedBlock != nullptr);
	if (selectedBlock->m_controllerID == VICUS::INVALID_ID)
		return;
	m_networkControllers.erase(m_networkControllers.begin() + controllerIndex(selectedBlock->m_controllerID));
	m_ui->controllerLineEdit->clear();
	m_sceneManager->setControllerID(selectedBlock, VICUS::INVALID_ID, "");
	m_sceneManager->update();
}


void SVSubNetworkEditDialog::on_copyBlockButton_clicked()
{
	if(m_sceneManager->selectedBlocks().size() == 0)
		return;
	VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().first());
	Q_ASSERT(selectedBlock != nullptr);
	if (selectedBlock->m_mode == VICUS::BMBlockType::GlobalInlet || selectedBlock->m_mode == VICUS::BMBlockType::GlobalOutlet || selectedBlock->m_mode == VICUS::BMBlockType::ConnectorBlock)
		return;
	else {
		VICUS::BMBlock newBlock = *selectedBlock;
		newBlock.m_pos += QPointF(2*VICUS::BLOCK_WIDTH, 0);

		// copy component
		VICUS::NetworkComponent component = m_networkComponents[componentIndex(selectedBlock->m_componentId)];
		component.m_id = newComponentID();
		newBlock.m_componentId = component.m_id;
		m_networkComponents.push_back(component);
		m_sceneManager->addBlock(newBlock);

		// copy controller, set new id
		if (selectedBlock->m_controllerID != VICUS::INVALID_ID){
			VICUS::NetworkController controller = m_networkControllers[controllerIndex(selectedBlock->m_controllerID)];
			controller.m_id = newControllerID();
			controller.m_builtIn = false;
			m_networkControllers.push_back(controller);
			newBlock.m_controllerID = controller.m_id;
			m_sceneManager->setControllerID(&m_sceneManager->network().m_blocks.back(), newBlock.m_controllerID,
														VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", controller.m_controlledProperty));
		}

		m_sceneManager->update();
	}
}

void SVSubNetworkEditDialog::on_componentSelected()
{
	// update enable state of button

	m_senderTable = qobject_cast<SVSubNetworkEditDialogTable*>(sender());
	if(m_senderTable == nullptr)
		return;

	int row = m_senderTable->currentRow();

	if(row == -1){
		m_ui->removeFromUserDBButton->setEnabled(false);
		return;
	}
	SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry entry = SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry(m_senderTable->m_elementList[(unsigned int)row]);
	if(entry.m_id != VICUS::INVALID_ID){
		m_ui->removeFromUserDBButton->setEnabled(!m_db->m_networkComponents[entry.m_id]->m_builtIn);
	} else {
		m_ui->removeFromUserDBButton->setEnabled(false);
	}
}


void SVSubNetworkEditDialog::on_addToUserDBButton_clicked()
{
	if(m_sceneManager->selectedBlocks().size() == 0)
		return;
	VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().first());
	Q_ASSERT(selectedBlock != nullptr);
	if(selectedBlock->m_mode == VICUS::BMBlockType::GlobalInlet || selectedBlock->m_mode == VICUS::BMBlockType::GlobalOutlet || selectedBlock->m_mode == VICUS::BMBlockType::ConnectorBlock)
		return;
	VICUS::NetworkComponent component = m_networkComponents[componentIndex(selectedBlock->m_componentId)];
	component.m_local = false;
	m_db->m_networkComponents.add(component) ;
	updateToolBoxPages();
}


void SVSubNetworkEditDialog::on_removeFromUserDBButton_clicked()
{
	if(m_senderTable == nullptr)
		return;
	int row = m_senderTable->currentRow();
	if(row == -1)
		return;

	SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry entry = SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry(m_senderTable->m_elementList[(unsigned int)row]);
	m_senderTable->clearSelection();
	m_db->m_networkComponents.remove(entry.m_id);
	updateToolBoxPages();
}

void SVSubNetworkEditDialog::on_projectSaved()
{
	QDir directory = QtExt::Directories::userDataDir() + "/thumbs/";
	QStringList filter;
	filter << "~SN*";
	directory.setNameFilters(filter);

	QStringList fileList = directory.entryList();
	for(QString file : fileList){
		qDebug() << "Files found: " << file;
		QString oldFileName = file;
		int index = file.indexOf("~SN");
		file.remove(index, 1);
		directory.rename(oldFileName, file);
	}
}

