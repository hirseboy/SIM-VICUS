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
#include "SVMainWindow.h"
#include "SVPreferencesDialog.h"
#include "SVPreferencesPageStyle.h"
#include "SVStyle.h"

#include <queue>
#include <QScreen>
#include <QKeyEvent>
#include <QDir>

#include <NANDRAD_HydraulicNetworkControlElement.h>
#include <NANDRAD_HydraulicNetworkComponent.h>

#include <IBK_MultiLanguageString.h>

#include <QtExt_Directories.h>
#include <QtExt_Style.h>
#include <QtExt_LanguageStringEditWidget1.h>

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


SVSubNetworkEditDialog::SVSubNetworkEditDialog(QWidget *parent, VICUS::SubNetwork * subNetwork, SVDatabase * db) :
	QWidget(parent),
	m_ui(new Ui::SVSubNetworkEditDialog)
{
	m_ui->setupUi(this);

	m_db = db;

	m_sceneManager = qobject_cast<SVBMSceneManager*>(m_ui->viewWidget->scene());
	setupSubNetwork(subNetwork);

	for(unsigned int i = 0; i < VICUS::NetworkComponent::ComponentCategory::NUM_CC; i++){
		m_tables.push_back(new SVSubNetworkEditDialogTable(this));
		connect(m_tables[i], &SVSubNetworkEditDialogTable::itemSelectionChanged, this, &SVSubNetworkEditDialog::on_componentSelected);
		connect(m_tables[i], &SVSubNetworkEditDialogTable::cellDoubleClicked, this, &SVSubNetworkEditDialog::on_componentDoubleClicked);
	}

	// populate tool box
	m_ui->tbox->blockSignals(true);
	m_ui->tbox->m_enableOpenMultiplePages = true;
	m_ui->tbox->layout()->setMargin(0);

	m_ui->tbox->addPage(tr("Pipes"), m_tables[0]);
	m_ui->tbox->addPage(tr("Pumps"), m_tables[1]);
	m_ui->tbox->addPage(tr("Heatpumps"), m_tables[2]);
	m_ui->tbox->addPage(tr("Other"), m_tables[3]);
	m_ui->tbox->blockSignals(false);
	m_ui->tbox->setCurrentIndex(0);

	m_ui->viewWidget->setDragMode(QGraphicsView::ScrollHandDrag);
	m_ui->viewWidget->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	m_ui->viewWidget->setResolution(1);
	m_ui->viewWidget->setStyleSheet("background-color: white;");

	m_ui->controllerLineEdit->setDisabled(true);

	m_ui->frameBuiltIn->setStyleSheet(QString(".QFrame { background-color: %1; }").arg(QtExt::Style::ToolBoxPageBackground));
	m_ui->frameUserDB->setStyleSheet(QString(".QFrame { background-color: %1; }").arg(SVStyle::instance().m_userDBBackgroundBright.name()));

	connect(m_sceneManager, &SVBMSceneManager::newBlockSelected, this, &SVSubNetworkEditDialog::blockSelectedEvent);
	connect(m_sceneManager, &SVBMSceneManager::newConnectorSelected, this, &SVSubNetworkEditDialog::connectorSelectedEvent);
	connect(m_sceneManager, &SVBMSceneManager::selectionCleared, this, &SVSubNetworkEditDialog::selectionClearedEvent);
	connect(m_sceneManager, &SVBMSceneManager::newBlockAdded, this, &SVSubNetworkEditDialog::on_newBlockAdded);

	connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &SVSubNetworkEditDialog::on_buttonBox_accepted);
	connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &SVSubNetworkEditDialog::on_buttonBox_rejected);
	connect(m_ui->nameLineEdit, &QLineEdit::textChanged, this, &SVSubNetworkEditDialog::on_NameTextChanged);

	connect(SVMainWindow::instance().preferencesDialog()->pageStyle(), &SVPreferencesPageStyle::styleChanged, m_ui->tbox, &QtExt::ToolBox::updatePageBackgroundColorFromStyle);
	connect(SVMainWindow::instance().preferencesDialog()->pageStyle(), &SVPreferencesPageStyle::styleChanged, this, &SVSubNetworkEditDialog::on_styleChanged);

	connect(&SVProjectHandler::instance(), &SVProjectHandler::projectSaved, this, &SVSubNetworkEditDialog::on_projectSaved);

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
}

void SVSubNetworkEditDialog::show()
{
	QWidget::show();

	// resize window
	QScreen *screen = QGuiApplication::primaryScreen();
	Q_ASSERT(screen!=nullptr);
	QRect rect = screen->geometry();
	int width = int(rect.width()* 0.9);
	int height = int(0.7*rect.height());

	resize(width, height);

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
	updateToolBoxPages();

}

void SVSubNetworkEditDialog::updateToolBoxPages(){

	for(SVSubNetworkEditDialogTable *table : m_tables) {
		table->clearSelection();
		table->clear();
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

	checkValidityOfNetworkElements();

	bool keepGraphicalNetwork = checkValidityOfNetworkElementsAndGraphicalNetwork();
	if (!keepGraphicalNetwork)
		createNewScene();
	else if(m_subNetwork->m_graphicalNetwork.m_blocks.size() == 0){
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
	} else
		m_sceneManager->updateNetwork(m_subNetwork->m_graphicalNetwork);

	// add Controller displayName to BlockItem
	for (auto& block : m_sceneManager->network().m_blocks){
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

	/* checks if a block is connected to itself */
	for(const VICUS::BMBlock &block : m_sceneManager->network().m_blocks){
		if (block.m_mode == VICUS::BMBlockType::NetworkComponentBlock){
			if(block.m_sockets[0].m_id == block.m_sockets[1].m_id){
				QMessageBox::warning(this, tr("Invalid Network"), tr("Block connected with itself. Invalid networks can not be saved."));
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

	// find cycles to check for "shortcircuiting" of parts of the subnetwork
	// first we will find all cycles in the graph, then we will check if the cycle "reattaches" to the remaining subnetwork in one spot
	// such that a short circuit is created
	// based on https://www.baeldung.com/cs/detecting-cycles-in-directed-graph

	//reuse previous connections graph
	std::map<int, bool> visited;
	// ID -1 denotes globalInlet, ID -2 is globalOutlet
	visited[-1] = false;
	visited[-2] = false;

	// fill up visited map with all available Block IDs
	for(const VICUS::BMBlock &block : m_sceneManager->network().m_blocks){
		int id = block.m_name.toInt();
		visited[id] = false;
	}

	for(auto mapIterator = visited.begin(); mapIterator != visited.end(); mapIterator++){
		qDebug() << "Block ID: " << mapIterator->first;
	}
	// build connection graph
	std::map<int, std::set<int>> neighbors;

	// first find all neighbors of the globalInlet
	for(const VICUS::BMConnector &con : m_sceneManager->network().m_connectors){
		if(!con.m_sourceSocket.contains(VICUS::SUBNETWORK_INLET_NAME) && !con.m_targetSocket.contains(VICUS::SUBNETWORK_INLET_NAME)) continue;

		QString sourceSocket, targetSocket;
		bool sourceSocketBelongstoGlobalInlet = con.m_sourceSocket.contains(VICUS::SUBNETWORK_INLET_NAME);
		sourceSocket = sourceSocketBelongstoGlobalInlet ? con.m_sourceSocket : con.m_targetSocket;
		targetSocket = sourceSocketBelongstoGlobalInlet ? con.m_targetSocket : con.m_sourceSocket;

		if(targetSocket.contains(VICUS::CONNECTORBLOCK_NAME)){
			QString nameOfConnectorBlock = targetSocket.split(".")[0];
			for(const VICUS::BMConnector& tmpCon : m_sceneManager->network().m_connectors){
				if(!tmpCon.m_sourceSocket.contains(nameOfConnectorBlock)) continue;

				QString ConnectorSourceSocket, ConnectorTargetSocket;
				bool sourceSocketBelongsToConnectorBlock = tmpCon.m_sourceSocket.contains(VICUS::CONNECTORBLOCK_NAME);
				ConnectorSourceSocket = sourceSocketBelongsToConnectorBlock ? tmpCon.m_sourceSocket : tmpCon.m_targetSocket;
				ConnectorTargetSocket = sourceSocketBelongsToConnectorBlock ? tmpCon.m_targetSocket : tmpCon.m_sourceSocket;

				neighbors[-1].insert(ConnectorTargetSocket.split(".")[0].toInt());
			}
		} else {
			neighbors[-1].insert(targetSocket.split(".")[0].toInt());
		}
	}

	// then find all neighbors of all other NetworkComponentBlocks
	for(const VICUS::BMBlock &block : m_sceneManager->network().m_blocks){
		if(block.m_mode != VICUS::NetworkComponentBlock) continue;
		int id = block.m_name.toInt();
		for(const VICUS::BMConnector &con : m_sceneManager->network().m_connectors){
			if(con.m_sourceSocket != block.m_name + "." + VICUS::OUTLET_NAME && con.m_targetSocket != block.m_name+ "." + VICUS::OUTLET_NAME) continue;

			QString sourceSocket, targetSocket;
			bool sourceSocketBelongsToBlock = con.m_sourceSocket.contains(block.m_name + "." + VICUS::OUTLET_NAME);
			sourceSocket = sourceSocketBelongsToBlock ? con.m_sourceSocket : con.m_targetSocket;
			targetSocket = sourceSocketBelongsToBlock ? con.m_targetSocket : con.m_sourceSocket;

			if(targetSocket.contains(VICUS::CONNECTORBLOCK_NAME)){
				QString nameOfConnectorBlock = targetSocket.split(".")[0];
				for(const VICUS::BMConnector& tmpCon : m_sceneManager->network().m_connectors){
					if(!tmpCon.m_sourceSocket.contains(nameOfConnectorBlock + "." + VICUS::OUTLET_NAME) && !tmpCon.m_targetSocket.contains(nameOfConnectorBlock+ "." + VICUS::OUTLET_NAME)) continue;

					QString ConnectorSourceSocket, ConnectorTargetSocket;
					bool sourceSocketBelongsToConnectorBlock = tmpCon.m_sourceSocket.contains(nameOfConnectorBlock);
					ConnectorSourceSocket = sourceSocketBelongsToConnectorBlock ? tmpCon.m_sourceSocket : tmpCon.m_targetSocket;
					ConnectorTargetSocket = sourceSocketBelongsToConnectorBlock ? tmpCon.m_targetSocket : tmpCon.m_sourceSocket;

					QString idOfNeighbor = ConnectorTargetSocket.split(".")[0];
					if(idOfNeighbor.contains(VICUS::SUBNETWORK_OUTLET_NAME)){
						neighbors[id].insert(-2);
					} else {
						neighbors[id].insert(idOfNeighbor.toInt());
					}
				}
			} else {
				QString idOfNeighbor = targetSocket.split(".")[0];
				if(idOfNeighbor.contains(VICUS::SUBNETWORK_OUTLET_NAME)){
					neighbors[id].insert(-2);
				} else {
					neighbors[id].insert(idOfNeighbor.toInt());
				}
			}
		}
	}

	qDebug() << "Mapping of Blocks to Neighbors";
	for(auto mapIterator : neighbors){
		qDebug() << "Block: " << mapIterator.first << ":";
		for(unsigned idIt : mapIterator.second){
			qDebug() << "		" << idIt;
		}
	}

	std::function<void(int, std::vector<int>&, std::set<int>&)> printCycle;
	printCycle = [&visited](int startVertex, std::vector<int>& stack, std::set<int>& localVisited) {
		qDebug() << "Cycle found!";
	};

	std::function<bool(int)> processDFSTree = [&](int v) {
		std::vector<int> stack;
		std::set<int> localVisited;
		bool cycleFound = false;
		std::function<void(int)> dfs = [&](int current) {
			visited[current] = true;
			localVisited.insert(current);
			stack.push_back(current);

			for (int u : neighbors[current]) {
				if (std::find(stack.begin(), stack.end(), u) != stack.end()) {
					cycleFound = true;
					return;
				} else if (!visited[u]) {
					dfs(u);
				}
			}

			stack.pop_back();
			visited[v] = false;
		};

		return cycleFound;
	};

	bool foundCycle;
	for (const auto& pair : neighbors) {
		if (!visited[pair.first]) {
			foundCycle = processDFSTree(pair.first);
			if(foundCycle) {
				QMessageBox::warning(this, tr("Invalid Network"), tr("Not all Blocks are reachable from globalInlet and globalOutlet. Invalid networks can not be saved."));
				return false;
			}
		}
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
	QString thumbNailPath = QtExt::Directories::userDataDir()  + "/thumbs/subnetworks/";
	if (!QDir(thumbNailPath).exists())
		QDir().mkdir(thumbNailPath);

	QString thumbName = SVProjectHandler::instance().projectFile();
	thumbName.replace("/", "_");
	thumbName.replace("\\", "_");
	thumbName.replace(":", "_");
	QString thumbPath = thumbNailPath + QString("~SN%1#%2.png").arg(thumbName).arg(m_subNetwork->m_id);
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
		if (block.m_mode == VICUS::BMBlockType::NetworkComponentBlock)
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
	for (unsigned int id : usedControllerIds){
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

void SVSubNetworkEditDialog::on_buttonBox_rejected() {
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


void SVSubNetworkEditDialog::connectorSelectedEvent(const QString & sourceSocketName, const QString & targetSocketName) {
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

void SVSubNetworkEditDialog::convertSubnetwork() {
	FUNCID(SVSubNetworkEditDialog::convertSubnetwork);
	// iterates over every element and copies component into Subnetwork and create new IDs
	for(VICUS::NetworkElement &element : m_subNetwork->m_elements){
		if(element.m_componentId != VICUS::INVALID_ID){
			VICUS::NetworkComponent *componentPtr = m_db->m_networkComponents[element.m_componentId];
			if (componentPtr == nullptr)
				throw IBK::Exception(tr("Could not find network component #%1.")
										 .arg(element.m_componentId).toStdString(), FUNC_ID);
			VICUS::NetworkComponent component = *componentPtr;
			component.m_id = VICUS::largestUniqueId(m_subNetwork->m_components);
			element.m_componentId = component.m_id;
			m_subNetwork->m_components.push_back(component);
		} else {
			throw IBK::Exception(tr("Invalid network component #%1.")
									 .arg(element.m_componentId).toStdString(), FUNC_ID);
		}
		// checks if element has a controller, if yes, copy it into subNetwork and create new IDs
		if(element.m_controlElementId != VICUS::INVALID_ID){
			VICUS::NetworkController *controllerPtr = m_db->m_networkControllers[element.m_controlElementId];
			if (controllerPtr == nullptr)
				throw IBK::Exception(tr("Could not find network controller #%1.")
										 .arg(element.m_controlElementId).toStdString(), FUNC_ID);
			VICUS::NetworkController controller = *controllerPtr;
			controller.m_id = VICUS::largestUniqueId(m_subNetwork->m_controllers);
			element.m_controlElementId = controller.m_id;
			m_subNetwork->m_controllers.push_back(controller);
		}
	}

	m_db->removeNotReferencedLocalElements(SVDatabase::DT_NetworkComponents, SVProjectHandler::instance().project());
	m_db->removeNotReferencedLocalElements(SVDatabase::DT_NetworkControllers, SVProjectHandler::instance().project());
}

void SVSubNetworkEditDialog::openDBComponentNamingDialog(VICUS::NetworkComponent* component)
{
	FUNCID(SVSubNetworkEditDialog::openDBComponentNamingDialog);

	// open dialog
	QDialog *namingDialog = new QDialog(this);
	namingDialog->setWindowTitle(tr("Set Component Name"));
	QVBoxLayout *layout = new QVBoxLayout(namingDialog);
	QLabel *label = new QLabel(tr("Please enter a name for the Component"));
	layout->addWidget(label);
	QtExt::LanguageStringEditWidget1 *editWidget = new QtExt::LanguageStringEditWidget1();
	layout->addWidget(editWidget);
	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(buttonBox);
	connect(buttonBox, SIGNAL(accepted()), namingDialog, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), namingDialog, SLOT(reject()));

	// get name from DB
	editWidget->setString(component->m_displayName);

	// show dialog
	if(namingDialog->exec() == QDialog::Accepted){
		// save name to DB
		component->m_displayName = editWidget->string();
	}
}

void SVSubNetworkEditDialog::checkValidityOfNetworkElements() {
	FUNCID(SVSubNetworkEditDialog::checkValidityOfNetworkElements);
	// Check that NetworkElement is not connected to itself
	// Check if for each NetworkElement that references a NetworkComponent, there is a NetworkComponent
	// Check if for each NetworkElement that references a NetworkController, there is a NetworkController
	for(const VICUS::NetworkElement& element: m_subNetwork->m_elements){
		if(element.m_componentId != VICUS::INVALID_ID){
			if(componentIndex(element.m_componentId) == VICUS::INVALID_ID){
				throw IBK::Exception(tr("NetworkElement contains reference to non existent NetworkComponent #%1.").arg(element.m_componentId).toStdString(), FUNC_ID);
			}
		}
		if(element.m_controlElementId != VICUS::INVALID_ID){
			if(controllerIndex(element.m_controlElementId) == VICUS::INVALID_ID){
				throw IBK::Exception(tr("NetworkElement contains reference to non existent NetworkController #%1.").arg(element.m_controlElementId).toStdString(), FUNC_ID);
			}
		}
	}
}

bool SVSubNetworkEditDialog::checkValidityOfNetworkElementsAndGraphicalNetwork() {

	// Check if there are as many NetworkComponentBlocks as NetworkElements
	int sizeOfNetworkElements = m_subNetwork->m_elements.size();

	int countNetworkElementBlocks = 0;
	for(const VICUS::BMBlock& block: m_subNetwork->m_graphicalNetwork.m_blocks){
		if(block.m_mode == VICUS::BMBlockType::NetworkComponentBlock)
			countNetworkElementBlocks++;
	}

	if(countNetworkElementBlocks != sizeOfNetworkElements){
		IBK::IBK_Message(tr("The network contains invalid elements. Please check the network.").toStdString());
		return false;
	}

	// Check if for each NetworkElement, there is one NetworkComponentBlock, check if inlet and outlet IDs are correct
	for(const VICUS::NetworkElement& element: m_subNetwork->m_elements){
		bool toAccept = false;
		for(const VICUS::BMBlock& block: m_subNetwork->m_graphicalNetwork.m_blocks){
			if(block.m_mode == VICUS::BMBlockType::NetworkComponentBlock && element.m_id == block.m_name.toUInt()){
				if(block.m_componentId == element.m_componentId && block.m_sockets[0].m_id == element.m_inletNodeId && block.m_sockets[1].m_id == element.m_outletNodeId){
					toAccept = true;
					break;
				}
			}
		}
		if(!toAccept){
			IBK::IBK_Message(tr("The NetworkElement and its block are incompatible or there is no block belonging to the NetworkElement. Please check the network.").toStdString());
			return false;
		}
	}

	std::set<unsigned int> alreadyVisited;
	// Check if for each connection of a NetworkElement, there is an equivalent BMConnection
	for(const VICUS::NetworkElement& element : m_subNetwork->m_elements){
		unsigned int inletNodeId = element.m_inletNodeId;

		// if ID not previously found, create set with all networkElements that are connected to this element
		if (alreadyVisited.find(inletNodeId) == alreadyVisited.end() && inletNodeId != VICUS::ENTRANCE_ID){
			alreadyVisited.insert(inletNodeId);

			// Loop over all NetworkElements, look for all NetworkElements that are connected to the inletNode
			std::set<const VICUS::NetworkElement*> connectedElements;
			bool notConnected = true;
			for(const VICUS::NetworkElement& element2 : m_subNetwork->m_elements){
				if(element2.m_inletNodeId == inletNodeId || element2.m_outletNodeId == inletNodeId){
					connectedElements.insert(&element2);
					notConnected = false;
				}
			}
			// if no connection found and inlet node id is not entrance ID, return
			if (notConnected){
				IBK::IBK_Message(tr("NetworkElement contains connection to non existent NetworkElement. Please check the network.").toStdString());
				return false;
			}

			// check if node is connected to ConnectorBlock or is directly connected
			// if directly connected, check if ENTRANCE_ID and construct source and targetName accordingly
			if(connectedElements.size() <= 2 && element.m_inletNodeId != VICUS::ENTRANCE_ID && element.m_inletNodeId != VICUS::EXIT_ID && element.m_outletNodeId != VICUS::ENTRANCE_ID && element.m_outletNodeId != VICUS::EXIT_ID ){
				bool foundConnector = false;
				QString targetName = QString::number(element.m_id) + "." + VICUS::INLET_NAME;
				QString sourceName;
				// find correct connected Element to get the name for sourceName
				const VICUS::NetworkElement* elementForSourceName = nullptr;
				for(const VICUS::NetworkElement* elementTmp : connectedElements){
					if(elementTmp->m_outletNodeId == inletNodeId)
						elementForSourceName = elementTmp;
				}
				Q_ASSERT(elementForSourceName != nullptr);
				sourceName = QString::number(elementForSourceName->m_id) + "." + VICUS::OUTLET_NAME;

				for(VICUS::BMConnector &connector : m_subNetwork->m_graphicalNetwork.m_connectors){
					if(connector.m_targetSocket == targetName && connector.m_sourceSocket == sourceName){
						foundConnector = true;
						break;
					}
				}
				if(!foundConnector)
					return false;
			}
		}
	}

	//Check if globalInlet is connected, verify that all necessary BMConnections exist
	std::vector<const VICUS::NetworkElement*> connectedElementsGlobalInlet;
	for(const VICUS::NetworkElement& element : m_subNetwork->m_elements){
		if(element.m_inletNodeId == VICUS::ENTRANCE_ID || element.m_outletNodeId == VICUS::ENTRANCE_ID)
			connectedElementsGlobalInlet.push_back(&element);
	}

	if(connectedElementsGlobalInlet.size() == 0 && m_subNetwork->m_elements.size() != 0){
		IBK::IBK_Message(tr("GlobalInlet not connected").toStdString());
		return false;
	} else if(connectedElementsGlobalInlet.size() == 1){

		QString sourceName;
		QString targetName;

		sourceName = VICUS::SUBNETWORK_INLET_NAME + "." + VICUS::OUTLET_NAME;
		targetName = QString::number(connectedElementsGlobalInlet[0]->m_id) + "." + VICUS::INLET_NAME;

		bool foundConnector = false;
		for(VICUS::BMConnector &connector : m_subNetwork->m_graphicalNetwork.m_connectors){
			if (connector.m_targetSocket == targetName && connector.m_sourceSocket == sourceName){
				foundConnector = true;
				break;
			}
		}
		if(!foundConnector)
			return false;
	}
	else {
		QString sourceName;
		QString targetName;
		// determine if connectorBlock has globalInlet as inlet or another NetworkElementBlock

		for(const VICUS::NetworkElement* element : connectedElementsGlobalInlet){
			bool inletConnectorBlock = element->m_inletNodeId == VICUS::ENTRANCE_ID ? true : false;

			if(inletConnectorBlock){
				sourceName = VICUS::CONNECTORBLOCK_NAME + QString::number(VICUS::ENTRANCE_ID) + "." + VICUS::OUTLET_NAME;
				targetName = QString::number(element->m_id) + "." + VICUS::INLET_NAME;
			} else {
				targetName = sourceName = VICUS::CONNECTORBLOCK_NAME + QString::number(VICUS::ENTRANCE_ID) + "." + VICUS::INLET_NAME;
				sourceName = QString::number(element->m_id) + "." + VICUS::OUTLET_NAME;
			}

			bool foundConnector = false;
			for(VICUS::BMConnector &connector : m_subNetwork->m_graphicalNetwork.m_connectors){
				if(connector.m_targetSocket == targetName && connector.m_sourceSocket == sourceName){
					foundConnector = true;
					break;
				}
			}
			if(!foundConnector)
				return false;
		}
	}


	//Check if globalOutlet is connected, verify that all necessary BMConnections exist
	std::vector<const VICUS::NetworkElement*> connectedElementsGlobalOutlet;
	for(const VICUS::NetworkElement& element : m_subNetwork->m_elements){
		if(element.m_outletNodeId == VICUS::EXIT_ID || element.m_inletNodeId == VICUS::EXIT_ID)
			connectedElementsGlobalOutlet.push_back(&element);
	}

	if(connectedElementsGlobalOutlet.size() == 0 && m_subNetwork->m_elements.size() != 0){
		IBK::IBK_Message(tr("GlobalOutlet not connected").toStdString());
		return false;
	} else if(connectedElementsGlobalOutlet.size() == 1){

		QString sourceName;
		QString targetName;

		sourceName = QString::number(connectedElementsGlobalOutlet[0]->m_id) + "." + VICUS::OUTLET_NAME;
		targetName = VICUS::SUBNETWORK_OUTLET_NAME + "." + VICUS::INLET_NAME;

		bool foundConnector = false;
		for(VICUS::BMConnector &connector : m_subNetwork->m_graphicalNetwork.m_connectors){
			if(connector.m_targetSocket == targetName && connector.m_sourceSocket == sourceName){
				foundConnector = true;
				break;
			}
		}
		if(!foundConnector)
			return false;
	}
	else {
		QString sourceName;
		QString targetName;

		targetName = VICUS::CONNECTORBLOCK_NAME + QString::number(VICUS::EXIT_ID) + "." + VICUS::INLET_NAME;
		for(const VICUS::NetworkElement* element : connectedElementsGlobalOutlet){
			sourceName = QString::number(element->m_id) + "." + VICUS::OUTLET_NAME;

			bool foundConnector = false;
			for(VICUS::BMConnector &connector : m_subNetwork->m_graphicalNetwork.m_connectors){
				bool inletConnectorBlock = element->m_inletNodeId == VICUS::EXIT_ID ? true : false;

				if(inletConnectorBlock){
					sourceName = VICUS::CONNECTORBLOCK_NAME + QString::number(VICUS::EXIT_ID) + "." + VICUS::OUTLET_NAME;
					targetName = QString::number(element->m_id) + "." + VICUS::INLET_NAME;
				} else {
					targetName = sourceName = VICUS::CONNECTORBLOCK_NAME + QString::number(VICUS::EXIT_ID) + "." + VICUS::INLET_NAME;
					sourceName = QString::number(element->m_id) + "." + VICUS::OUTLET_NAME;
				}

				if(connector.m_targetSocket == targetName && connector.m_sourceSocket == sourceName){
					foundConnector = true;
					break;
				}
			}
			if(!foundConnector)
				return false;
		}
	}

	return true;
}

void SVSubNetworkEditDialog::createNewScene()
{
	m_subNetwork->m_graphicalNetwork.m_blocks.clear();
	m_subNetwork->m_graphicalNetwork.m_connectors.clear();

	m_sceneManager->updateNetwork(m_subNetwork->m_graphicalNetwork);

	//first create GlobalInlet and GlobalOutlet Blocks
	VICUS::BMBlock bentry, bexit;
	bentry.m_name = VICUS::SUBNETWORK_INLET_NAME;
	bentry.m_mode = VICUS::BMBlockType::GlobalInlet;
	bentry.m_size = QSizeF(VICUS::ENTRANCEEXITBLOCK_WIDTH,VICUS::ENTRANCEEXITBLOCK_HEIGHT);
	bentry.m_pos = QPointF(0, 400);

	bexit.m_name = VICUS::SUBNETWORK_OUTLET_NAME;
	bexit.m_mode = VICUS::BMBlockType::GlobalOutlet;
	bexit.m_size = QSizeF(VICUS::ENTRANCEEXITBLOCK_WIDTH,VICUS::ENTRANCEEXITBLOCK_HEIGHT);
	bexit.m_pos = QPointF(1102, 400);

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

	/* groups blocks together by node ID and spaces them out accordingly by the number of different node Ids.
	 * Ignores globalInlet and globalOutlet for now */
	std::vector<unsigned int> listContainingAllNodeIds;
	std::map<unsigned int, VICUS::BMBlock> mapWithAllBlocks;

	// keeps track of all blocks placed on the scene in case a block is left out because it is only connected to the global inlet/outlet
	std::map<unsigned int, bool> isBlockAlreadyPlacedOnScene;

	std::vector<unsigned int> blocksConnectedToGlobalOutlet;
	std::vector<unsigned int> blocksConnectedToGlobalInlet;

	// create blocks for each node, initialises isBlockAlreadyPlacedOnScene to false
	for(const VICUS::NetworkElement &element : m_subNetwork->m_elements){

		isBlockAlreadyPlacedOnScene[element.m_id] = false;

		VICUS::BMBlock block = VICUS::BMBlock(QString::number(element.m_id));
		VICUS::BMSocket outlet, inlet;
		block.m_componentId = element.m_componentId;
		block.m_controllerID = element.m_controlElementId;

		VICUS::NetworkComponent *comp = VICUS::element(m_subNetwork->m_components, element.m_componentId);
		Q_ASSERT(comp!=nullptr);
		VICUS::NetworkComponent::ModelType type = comp->m_modelType;

		block.m_displayName = element.m_displayName;
		block.m_mode = VICUS::BMBlockType::NetworkComponentBlock;
		block.m_size = QSizeF(VICUS::BLOCK_WIDTH, VICUS::BLOCK_HEIGHT);
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

		mapWithAllBlocks[element.m_id] = block;
		if ( std::find(listContainingAllNodeIds.begin(), listContainingAllNodeIds.end(), element.m_inletNodeId) == listContainingAllNodeIds.end()){
			listContainingAllNodeIds.push_back(element.m_inletNodeId);
		}
		if ( std::find(listContainingAllNodeIds.begin(), listContainingAllNodeIds.end(), element.m_outletNodeId) == listContainingAllNodeIds.end()){
			listContainingAllNodeIds.push_back(element.m_outletNodeId);
		}

		if (element.m_outletNodeId == VICUS::EXIT_ID || element.m_inletNodeId == VICUS::EXIT_ID){
			blocksConnectedToGlobalOutlet.push_back(element.m_id);
		}

		if (element.m_outletNodeId == VICUS::ENTRANCE_ID || element.m_inletNodeId == VICUS::ENTRANCE_ID){
			blocksConnectedToGlobalInlet.push_back(element.m_id);
		}
	}

	// calculates the space between each blocks based on the number of different node IDs
	int distancePerBlock = (1102 - VICUS::ENTRANCEEXITBLOCK_WIDTH) / listContainingAllNodeIds.size() - VICUS::CONNECTORBLOCK_WIDTH;

	/* iterates over all node IDs, groups blocks by what socket is connected to the node ID
	 * and places them on the scene accordingly */
	for (unsigned int i = 0; i < listContainingAllNodeIds.size(); i++){
		unsigned int nodeId = listContainingAllNodeIds[i];
		// skip over blocks connected to inlet and outlet
		if (nodeId == VICUS::ENTRANCE_ID || nodeId == VICUS::EXIT_ID) continue;
		std::vector<VICUS::BMBlock*> outletNodeBlocks;
		std::vector<VICUS::BMBlock*> inletNodeBlocks;
		for (const VICUS::NetworkElement &element : m_subNetwork->m_elements){
			if (element.m_inletNodeId == nodeId){
				inletNodeBlocks.push_back(&mapWithAllBlocks[element.m_id]);

			} else if (element.m_outletNodeId == nodeId){
				outletNodeBlocks.push_back(&mapWithAllBlocks[element.m_id]);
			}
		}

		for (unsigned int j = 0; j < outletNodeBlocks.size(); j++){
			VICUS::BMBlock *block = outletNodeBlocks[j];
			if (!isBlockAlreadyPlacedOnScene[block->m_name.toUInt()]){
				block->m_pos = QPointF(VICUS::ENTRANCEEXITBLOCK_WIDTH + (i) * distancePerBlock, 120 * j + 392  - 120 * ( outletNodeBlocks.size() / 2));
				m_sceneManager->addBlock(*block);
				isBlockAlreadyPlacedOnScene[block->m_name.toUInt()] = true;
			}
		}

		for (unsigned int j = 0; j < inletNodeBlocks.size(); j++){
			VICUS::BMBlock *block = inletNodeBlocks[j];
			if (!isBlockAlreadyPlacedOnScene[block->m_name.toUInt()]){
				block->m_pos = QPointF(VICUS::BLOCK_WIDTH + (i+1) * distancePerBlock, 120 * (j) + 392 - 120 * ( inletNodeBlocks.size() / 2));
				m_sceneManager->addBlock(*block);
				isBlockAlreadyPlacedOnScene[block->m_name.toUInt()] = true;
			}
		}

		if (outletNodeBlocks.size() + inletNodeBlocks.size() <= 2){
			m_sceneManager->createConnection(outletNodeBlocks[0], inletNodeBlocks[0], &outletNodeBlocks[0]->m_sockets[1], &inletNodeBlocks[0]->m_sockets[0]);

		} else {
			// Create connector block and socket
			// calculate position in the center between source and target block
			VICUS::BMBlock connectorBlock(VICUS::CONNECTORBLOCK_NAME + QString::number(nodeId),
										  outletNodeBlocks[0]->m_pos.x() + VICUS::BLOCK_WIDTH + (inletNodeBlocks[0]->m_pos.x() - outletNodeBlocks[0]->m_pos.x() - VICUS::BLOCK_WIDTH ) / 2,
										  392 + VICUS::ENTRANCEEXITBLOCK_HEIGHT / 2);
			connectorBlock.m_mode = VICUS::BMBlockType::ConnectorBlock;
			connectorBlock.m_size = QSizeF(VICUS::CONNECTORBLOCK_WIDTH, VICUS::CONNECTORBLOCK_HEIGHT);
			VICUS::BMSocket inlet(VICUS::INLET_NAME, QPointF(0, connectorBlock.m_size.height() / 2), Qt::Horizontal, true);
			inlet.m_id = nodeId;
			inlet.m_isSocketOfConnector = true;
			VICUS::BMSocket outlet(VICUS::OUTLET_NAME, QPointF(connectorBlock.m_size.width() / 2, connectorBlock.m_size.height() / 2), Qt::Horizontal, false);
			outlet.m_id = nodeId;
			outlet.m_isSocketOfConnector = true;
			connectorBlock.m_sockets.append(inlet);
			connectorBlock.m_sockets.append(outlet);

			m_sceneManager->addConnectorBlock(connectorBlock);

			for (unsigned int j = 0; j < outletNodeBlocks.size(); j++){
				m_sceneManager->createConnection(outletNodeBlocks[j], &connectorBlock, &outletNodeBlocks[j]->m_sockets[1], &connectorBlock.m_sockets[0]);
			}

			for (unsigned int j = 0; j < inletNodeBlocks.size(); j++){
				m_sceneManager->createConnection(&connectorBlock, inletNodeBlocks[j], &connectorBlock.m_sockets[1], &inletNodeBlocks[j]->m_sockets[0]);
			}
		}
	}


	// Now create connections to Global Outlet

	// Calculates position on the scene, adds to scene
	for (unsigned int i = 0; i < blocksConnectedToGlobalOutlet.size(); i++){
		if (isBlockAlreadyPlacedOnScene[blocksConnectedToGlobalOutlet[i]]){
			continue;
		}
		mapWithAllBlocks[blocksConnectedToGlobalOutlet[i]].m_pos = QPointF(1102 / 2, 120 * (i) + 392 - 120 * ( blocksConnectedToGlobalOutlet.size() / 2));
		m_sceneManager->addBlock(mapWithAllBlocks[blocksConnectedToGlobalOutlet[i]]);
		isBlockAlreadyPlacedOnScene[blocksConnectedToGlobalOutlet[i]] = true;
	}

	//Creates ConnectorBlock if necessary, connects all blocks accordingly
	if (blocksConnectedToGlobalOutlet.size() > 1){
		// Create connector block and socket
		// calculate position in the center between source and target block
		VICUS::BMBlock connectorBlock(VICUS::CONNECTORBLOCK_NAME + QString::number(VICUS::EXIT_ID),(
									bexit.m_pos.x() - mapWithAllBlocks[blocksConnectedToGlobalOutlet[0]].m_pos.x() - VICUS::BLOCK_WIDTH ) / 2 + mapWithAllBlocks[blocksConnectedToGlobalOutlet[0]].m_pos.x(),
									  392 + VICUS::ENTRANCEEXITBLOCK_HEIGHT / 2);
		connectorBlock.m_mode = VICUS::BMBlockType::ConnectorBlock;
		connectorBlock.m_size = QSizeF(VICUS::CONNECTORBLOCK_WIDTH, VICUS::CONNECTORBLOCK_HEIGHT);
		VICUS::BMSocket inlet(VICUS::INLET_NAME, QPointF(0, connectorBlock.m_size.height() / 2), Qt::Horizontal, true);
		inlet.m_id = VICUS::EXIT_ID;
		inlet.m_isSocketOfConnector = true;
		VICUS::BMSocket outlet(VICUS::OUTLET_NAME, QPointF(connectorBlock.m_size.width() / 2, connectorBlock.m_size.height() / 2), Qt::Horizontal, false);
		outlet.m_id = VICUS::EXIT_ID;
		outlet.m_isSocketOfConnector = true;
		connectorBlock.m_sockets.append(inlet);
		connectorBlock.m_sockets.append(outlet);

		m_sceneManager->addConnectorBlock(connectorBlock);

		for (unsigned int j = 0; j < blocksConnectedToGlobalOutlet.size(); j++){
			if (mapWithAllBlocks[blocksConnectedToGlobalOutlet[j]].m_sockets[0].m_id == VICUS::EXIT_ID){
				m_sceneManager->createConnection(&connectorBlock, &mapWithAllBlocks[blocksConnectedToGlobalOutlet[j]], &connectorBlock.m_sockets[1], &mapWithAllBlocks[blocksConnectedToGlobalOutlet[j]].m_sockets[0]);
			} else if (mapWithAllBlocks[blocksConnectedToGlobalOutlet[j]].m_sockets[1].m_id == VICUS::EXIT_ID){
				m_sceneManager->createConnection(&connectorBlock, &mapWithAllBlocks[blocksConnectedToGlobalOutlet[j]], &connectorBlock.m_sockets[0], &mapWithAllBlocks[blocksConnectedToGlobalOutlet[j]].m_sockets[1]);
			}
		}

		m_sceneManager->createConnection(&connectorBlock, &bexit , &connectorBlock.m_sockets[1], &bexit.m_sockets[0]);

	} else if (blocksConnectedToGlobalOutlet.size() == 1){
		m_sceneManager->createConnection(&mapWithAllBlocks[blocksConnectedToGlobalOutlet[0]], &bexit, &mapWithAllBlocks[blocksConnectedToGlobalOutlet[0]].m_sockets[1], &bexit.m_sockets[0]);
	}

	// Now create connections to Global Inlet

	// Calculates position on the scene, adds to scene
	for (unsigned int i = 0; i < blocksConnectedToGlobalInlet.size(); i++){
		if (isBlockAlreadyPlacedOnScene[blocksConnectedToGlobalInlet[i]]){
			continue;
		}
		mapWithAllBlocks[blocksConnectedToGlobalInlet[i]].m_pos = QPointF(1102 / 2, 120 * (i) - 392 - 120 * ( blocksConnectedToGlobalInlet.size() / 2));
		m_sceneManager->addBlock(mapWithAllBlocks[blocksConnectedToGlobalInlet[i]]);
		isBlockAlreadyPlacedOnScene[blocksConnectedToGlobalInlet[i]] = true;
	}

	//Creates ConnectorBlock if necessary, connects all blocks accordingly
	if (blocksConnectedToGlobalInlet.size() > 1){
		// Create connector block and socket
		// calculate position in the center between source and target block
		VICUS::BMBlock connectorBlock(VICUS::CONNECTORBLOCK_NAME + QString::number(VICUS::ENTRANCE_ID),(
																											mapWithAllBlocks[blocksConnectedToGlobalInlet[0]].m_pos.x()) / 2,
									  392 + VICUS::ENTRANCEEXITBLOCK_HEIGHT / 2);
		connectorBlock.m_mode = VICUS::BMBlockType::ConnectorBlock;
		connectorBlock.m_size = QSizeF(VICUS::CONNECTORBLOCK_WIDTH, VICUS::CONNECTORBLOCK_HEIGHT);
		VICUS::BMSocket inlet(VICUS::INLET_NAME, QPointF(0, connectorBlock.m_size.height() / 2), Qt::Horizontal, true);
		inlet.m_id = VICUS::EXIT_ID;
		inlet.m_isSocketOfConnector = true;
		VICUS::BMSocket outlet(VICUS::OUTLET_NAME, QPointF(connectorBlock.m_size.width() / 2, connectorBlock.m_size.height() / 2), Qt::Horizontal, false);
		outlet.m_id = VICUS::EXIT_ID;
		outlet.m_isSocketOfConnector = true;
		connectorBlock.m_sockets.append(inlet);
		connectorBlock.m_sockets.append(outlet);

		m_sceneManager->addConnectorBlock(connectorBlock);

		for (unsigned int j = 0; j < blocksConnectedToGlobalInlet.size(); j++){
			if (mapWithAllBlocks[blocksConnectedToGlobalInlet[j]].m_sockets[0].m_id == VICUS::ENTRANCE_ID){
				m_sceneManager->createConnection(&connectorBlock, &mapWithAllBlocks[blocksConnectedToGlobalInlet[j]], &connectorBlock.m_sockets[1], &mapWithAllBlocks[blocksConnectedToGlobalInlet[j]].m_sockets[0]);
			} else if (mapWithAllBlocks[blocksConnectedToGlobalInlet[j]].m_sockets[1].m_id == VICUS::ENTRANCE_ID){
				m_sceneManager->createConnection(&mapWithAllBlocks[blocksConnectedToGlobalInlet[j]], &connectorBlock, &mapWithAllBlocks[blocksConnectedToGlobalInlet[j]].m_sockets[1], &connectorBlock.m_sockets[0]);
			}
		}

		m_sceneManager->createConnection(&bentry, &connectorBlock , &bentry.m_sockets[0], &connectorBlock.m_sockets[0]);

	} else if (blocksConnectedToGlobalInlet.size() == 1){
		m_sceneManager->createConnection(&bentry, &mapWithAllBlocks[blocksConnectedToGlobalInlet[0]], &bentry.m_sockets[0], &mapWithAllBlocks[blocksConnectedToGlobalInlet[0]].m_sockets[0]);
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
		m_ui->toolButtonRename->setEnabled(false);
		return;
	}
	SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry entry = SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry(m_senderTable->m_elementList[(unsigned int)row]);
	if(entry.m_id != VICUS::INVALID_ID){
		m_ui->removeFromUserDBButton->setEnabled(!m_db->m_networkComponents[entry.m_id]->m_builtIn);
		m_ui->toolButtonRename->setEnabled(!m_db->m_networkComponents[entry.m_id]->m_builtIn);
	} else {
		m_ui->removeFromUserDBButton->setEnabled(false);
		m_ui->toolButtonRename->setEnabled(false);
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

	openDBComponentNamingDialog(&component);

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

void SVSubNetworkEditDialog::on_toolButtonRename_clicked()
{

	if(m_senderTable == nullptr)
		return;
	int row = m_senderTable->currentRow();
	if(row == -1)
		return;

	SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry entry = SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry(m_senderTable->m_elementList[(unsigned int)row]);

	VICUS::NetworkComponent* component = m_db->m_networkComponents[entry.m_id];
	Q_ASSERT(component != nullptr);
	openDBComponentNamingDialog(component);
	updateToolBoxPages();
}

void SVSubNetworkEditDialog::on_componentDoubleClicked()
{
	if(m_senderTable == nullptr)
		return;
	int row = m_senderTable->currentRow();
	if(row == -1)
		return;

	SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry entry = SVSubNetworkEditDialogTable::SubNetworkEditDialogTableEntry(m_senderTable->m_elementList[(unsigned int)row]);

	if(entry.m_id == VICUS::INVALID_ID)
		return;
	VICUS::NetworkComponent* component = m_db->m_networkComponents[entry.m_id];
	Q_ASSERT(component != nullptr);
	if(component->m_builtIn)
		return;
	openDBComponentNamingDialog(component);
	updateToolBoxPages();
}

void SVSubNetworkEditDialog::on_projectSaved()
{
	QDir directory = QtExt::Directories::userDataDir() + "/thumbs/subnetworks/";
	QStringList filter;
	filter << "~SN*";
	directory.setNameFilters(filter);

	QStringList fileList = directory.entryList();
	for(QString file : fileList){
		QString oldFileName = file;
		int index = file.indexOf("~SN");
		file.remove(index, 1);
		directory.remove(file);
		directory.rename(oldFileName, file);
	}
}

void SVSubNetworkEditDialog::on_styleChanged()
{
	qDebug() << "SVSubNetworkEditDialog::on_styleChanged()";
	m_ui->frameBuiltIn->setStyleSheet(QString(".QFrame { background-color: %1; }").arg(QtExt::Style::ToolBoxPageBackground));
	m_ui->frameUserDB->setStyleSheet(QString(".QFrame { background-color: %1; }").arg(SVStyle::instance().m_userDBBackgroundBright.name()));
	m_ui->tbox->layout()->setMargin(0);
	m_ui->tbox->layout()->setSpacing(0);
}
