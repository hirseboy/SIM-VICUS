#include "SVSubNetworkEditDialog.h"
#include "ui_SVSubNetworkEditDialog.h"

#include "QHBoxLayout"
#include "QPushButton"
#include <QRandomGenerator>
#include <QTextEdit>
#include <QDebug>
#include <queue>
#include <QLabel>
#include <QPixmap>
#include <QList>
#include <QString>
#include <QFileDialog>
#include <map>
#include <QMessageBox>
#include <QScreen>

#include "NANDRAD_HydraulicNetworkControlElement.h"
#include "NANDRAD_HydraulicNetworkComponent.h"

#include "IBK_MultiLanguageString.h"


#include <SVBMZoomMeshGraphicsView.h>
#include <SVBMSceneManager.h>
#include "SVSubNetworkEditDialogTable.h"
#include "SVDBSubNetworkEditWidget.h"
#include "SVDBNetworkControllerEditWidget.h"
#include "SVDatabase.h"
#include "SVSettings.h"

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
	QDialog(parent),
	ui(new Ui::SVSubNetworkEditDialog)
{
	ui->setupUi(this);

	QScreen *screen = QGuiApplication::primaryScreen();
	Q_ASSERT(screen!=nullptr);
	QRect rect = screen->geometry();
	int width = rect.width()* 0.95;
	int height =  0.8*rect.height();

	if(width < VICUS::MINIMUMDIALOGWIDTH){
		width = VICUS::MINIMUMDIALOGWIDTH;
	}
	if(height < VICUS::MINIMUMDIALOGHEIGHT){
		height = VICUS::MINIMUMDIALOGHEIGHT;
	}

	resize(width,height);
	ui->scrollArea->setMaximumHeight(height-20);


	m_db = db;
	SVSubNetworkEditDialog::createToolBox();
	ui->tbox->addPage(tr("Pipes"), m_tables[0]);
	ui->tbox->addPage(tr("Pumps"), m_tables[1]);
	ui->tbox->addPage(tr("Heatpumps"), m_tables[2]);
	ui->tbox->addPage(tr("Other"), m_tables[3]);
	ui->tbox->blockSignals(false);
	ui->tbox->setCurrentIndex(0);
	ui->viewWidget->setDragMode(QGraphicsView::ScrollHandDrag);
	ui->viewWidget->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	ui->viewWidget->setResolution(1);

	ui->controllerLineEdit->setDisabled(true);
	m_sceneManager = qobject_cast<SVBMSceneManager*>(ui->viewWidget->scene());
	setupSubNetwork(subNetwork);

	connect(m_sceneManager, &SVBMSceneManager::newBlockSelected,this, &SVSubNetworkEditDialog::blockSelectedEvent);
	connect(m_sceneManager, &SVBMSceneManager::newConnectorSelected,this, &SVSubNetworkEditDialog::connectorSelectedEvent);
	connect(m_sceneManager, &SVBMSceneManager::selectionCleared,this, &SVSubNetworkEditDialog::selectionClearedEvent);
	connect(m_sceneManager, &SVBMSceneManager::newBlockAdded, this, &SVSubNetworkEditDialog::on_newBlockAdded);
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SVSubNetworkEditDialog::on_buttonBox_accepted);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SVSubNetworkEditDialog::on_buttonBox_rejected);
	connect(ui->nameLineEdit, &QLineEdit::textChanged, this, &SVSubNetworkEditDialog::on_NameTextChanged);
}

SVSubNetworkEditDialog::~SVSubNetworkEditDialog()
{
	delete ui;
}

void SVSubNetworkEditDialog::setupSubNetwork(VICUS::SubNetwork *subNetwork)
{
	m_subNetwork = subNetwork;
	updateNetwork();
	ui->networkComponentEditWidget->setup(m_networkComponents, m_db);
	ui->networkComponentEditWidget->setFixedWidth(491);
}

void SVSubNetworkEditDialog::show()
{
	QDialog::show();

	QList<int> ratio;
	ratio << 300 << ui->splitter->width()-300;
	ui->splitter->setSizes(ratio);

	int viewWidth = ui->viewWidget->size().width();
	int viewHeight = ui->viewWidget->size().height();
	double aspectRatio = double(viewWidth)/double(viewHeight);
	int sceneWidth = 1151;
	int sceneHeight = int(sceneWidth/aspectRatio);
	m_sceneManager->setSceneRect(0,450-sceneHeight/2,sceneWidth,450+sceneHeight/2);
	ui->viewWidget->fitInView(m_sceneManager->sceneRect(), Qt::KeepAspectRatio);
	ui->networkComponentEditWidget->updateInput(-1);
	m_sceneManager->update();
}

void SVSubNetworkEditDialog::resize(int w, int h)
{
	QDialog::resize(QSize(w,h));
	ui->scrollArea->setMaximumHeight(h-20);
}

void SVSubNetworkEditDialog::createToolBox(){

	ui->tbox->m_mode = true;
	ui->tbox->layout()->setMargin(0);
	ui->tbox->blockSignals(true);

	if(m_tables.size() == 0){
		for(int i = 0; i < static_cast<int>(VICUS::NetComCategory::NUM_NC); i++){
			m_tables.push_back(new SVSubNetworkEditDialogTable(this));
			connect(m_tables[i], &SVSubNetworkEditDialogTable::itemSelectionChanged, this, &SVSubNetworkEditDialog::on_componentSelected);
		}
	} else {
		for(auto& table : m_tables){
			table->clearSelection();
			table->clear();
		}
	}

	//Lambda function to add further elements of type NetworkComponent::ModelType to the toolbox
	auto addElement = [&](VICUS::NetworkComponent::ModelType type, VICUS::NetComCategory category){
		qDebug() << "addElement" << type << category;
		auto networkComponents = m_db->m_networkComponents.begin();
		do{
			if(networkComponents->second.m_modelType == type){
				m_tables[static_cast<int>(category)]->addElement(networkComponents->second);
			}
			networkComponents++;
		}while(networkComponents != m_db->m_networkComponents.end());
	};

	for(const auto& pair : VICUS::ModelTypeNetComCategoryAttributes){
		switch(pair.second){
		case VICUS::Pipes:
			m_tables[static_cast<int>(VICUS::NetComCategory::Pipes)]->addElement(pair.first);
			addElement(pair.first, pair.second);
			break;
		case VICUS::Pumps:
			m_tables[static_cast<int>(VICUS::NetComCategory::Pumps)]->addElement(pair.first);
			addElement(pair.first, pair.second);
			break;
		case VICUS::Heatpumps:
			m_tables[static_cast<int>(VICUS::NetComCategory::Heatpumps)]->addElement(pair.first);
			addElement(pair.first, pair.second);
			break;
		case VICUS::Other:
			m_tables[static_cast<int>(VICUS::NetComCategory::Other)]->addElement(pair.first);
			addElement(pair.first, pair.second);
			break;
		default:
			qDebug() << "no entry for VICUS::NetworkComponent::ModelType " << pair.first;
		}
	}
	ui->tbox->update();

}

void SVSubNetworkEditDialog::updateNetwork()
{
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

		VICUS::BMBlock* previous = &bentry;

		for(VICUS::NetworkElement element : m_subNetwork->m_elements){
			VICUS::BMBlock* block = new VICUS::BMBlock(QString::number(counter));
			VICUS::BMSocket outlet, inlet;
			if(element.m_componentId != VICUS::INVALID_ID){
				VICUS::NetworkComponent component;
				component = *(m_db->m_networkComponents[element.m_componentId]);
				component.m_builtIn = false;
				component.m_id = createNewComponentID();
				block->m_componentId = component.m_id;
				component.m_builtIn = false;
				m_networkComponents.push_back(component);
			}
			if(element.m_controlElementId != VICUS::INVALID_ID){
				VICUS::NetworkController controller;
				controller = *(m_db->m_networkControllers[element.m_controlElementId]);
				controller.m_id = getNewControllerID();
				block->m_controllerID = controller.m_id;
				controller.m_builtIn = false;
				m_networkControllers.push_back(controller);
			}

			VICUS::NetworkComponent::ModelType type = m_networkComponents[block->m_componentId].m_modelType;

			block->m_displayName = element.m_displayName;
			block->m_mode = VICUS::BMBlockType::NetworkComponentBlock;
			block->m_size = QSizeF(VICUS::BLOCK_WIDTH, VICUS::BLOCK_HEIGHT);
			QPoint point(VICUS::ENTRANCEEXITBLOCK_WIDTH + (counter + 1) * distancePerBlock + counter * VICUS::CONNECTORBLOCK_WIDTH, 400);
			block->m_pos = point;
			block->m_properties["ShowPixmap"] = true;
			block->m_properties["Pixmap"] = QPixmap(VICUS::getIconFileFromModelType(type)).scaled(256,256);

			inlet.m_name = VICUS::INLET_NAME;
			inlet.m_isInlet = true;
			inlet.m_id = element.m_inletNodeId;
			inlet.m_orientation = Qt::Horizontal;
			inlet.m_pos = QPointF(0, VICUS::BLOCK_HEIGHT / 2);
			block->m_sockets.append(inlet);


			outlet.m_name = VICUS::OUTLET_NAME;
			outlet.m_isInlet = false;
			outlet.m_id = element.m_outletNodeId;
			outlet.m_orientation = Qt::Horizontal;
			outlet.m_pos = QPointF(VICUS::BLOCK_WIDTH, VICUS::BLOCK_HEIGHT / 2);
			block->m_sockets.append(outlet);

			m_sceneManager->addBlock(*block);

			if(element.m_inletNodeId == VICUS::ENTRANCE_ID){
				m_sceneManager->createConnection(previous, block, &(previous->m_sockets[0]), &(block->m_sockets[0]));
			}
			else{
				m_sceneManager->createConnection(previous, block, &(previous->m_sockets[1]), &(block->m_sockets[0]));
			}
			if(element.m_outletNodeId == VICUS::EXIT_ID){
				m_sceneManager->createConnection(block, &bexit, &(block->m_sockets[1]), &(bexit.m_sockets[0]));
			}
			previous = block;

			counter++;
		}

	}

	// if BMNetwork filled, create necessary graphical elements
	else if(m_subNetwork->m_graphicalNetwork.m_blocks.begin()->m_displayName == VICUS::INVALIDXMLREAD){
		for(auto &block : m_subNetwork->m_graphicalNetwork.m_blocks){
			bool ok;
			unsigned int number = block.m_name.toInt(&ok);
			if(ok){
				VICUS::NetworkElement element;
				for(auto elementTmp : m_subNetwork->m_elements){
					if(elementTmp.m_id == number){
						element = elementTmp;
						break;
					}
				}
				block.m_mode = VICUS::BMBlockType::NetworkComponentBlock;
				block.m_displayName = element.m_displayName;
				block.m_componentId = element.m_componentId;
				block.m_controllerID = element.m_controlElementId;
				block.m_size = QSizeF(VICUS::BLOCK_WIDTH, VICUS::BLOCK_HEIGHT);
				block.m_properties["ShowPixmap"] = true;
				block.m_properties["Pixmap"] = QPixmap(VICUS::getIconFileFromModelType(m_networkComponents[componentIndex(element.m_componentId)].m_modelType)).scaled(256,256);

				VICUS::BMSocket inlet, outlet;

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
			} else {
				if(block.m_name.contains(VICUS::CONNECTORBLOCK_NAME)){
					block.m_displayName = "";
					block.m_mode = VICUS::BMBlockType::ConnectorBlock;
					block.m_size = QSizeF(VICUS::CONNECTORBLOCK_WIDTH, VICUS::CONNECTORBLOCK_HEIGHT);
					VICUS::BMSocket inlet, outlet;

					inlet.m_name = VICUS::INLET_NAME;
					inlet.m_isInlet = true;
					inlet.m_id = outlet.m_id;
					inlet.m_orientation = Qt::Horizontal;
					inlet.m_pos = QPointF(0, VICUS::CONNECTORBLOCK_HEIGHT / 2);
					block.m_sockets.append(inlet);

					outlet.m_name = VICUS::OUTLET_NAME;
					outlet.m_isInlet = false;
					outlet.m_id = block.m_name.split("r")[1].toInt();
					outlet.m_orientation = Qt::Horizontal;
					outlet.m_pos = QPointF(VICUS::CONNECTORBLOCK_WIDTH, VICUS::CONNECTORBLOCK_HEIGHT / 2);
					block.m_sockets.append(outlet);
				} else if(block.m_name.contains(VICUS::SUBNETWORK_INLET_NAME)){
					block.m_size = QSizeF(VICUS::ENTRANCEEXITBLOCK_WIDTH, VICUS::ENTRANCEEXITBLOCK_HEIGHT);
					block.m_mode = VICUS::BMBlockType::GlobalInlet;
					block.m_displayName.clear();
					VICUS::BMSocket outlet;
					outlet.m_name = VICUS::OUTLET_NAME;
					outlet.m_isInlet = false;
					outlet.m_id = VICUS::ENTRANCE_ID;
					outlet.m_orientation = Qt::Horizontal;
					outlet.m_pos = QPointF(VICUS::ENTRANCEEXITBLOCK_WIDTH, VICUS::ENTRANCEEXITBLOCK_HEIGHT / 2);
					block.m_sockets.append(outlet);
				}
				else if(block.m_name.contains(VICUS::SUBNETWORK_OUTLET_NAME)){
					block.m_size = QSizeF(VICUS::ENTRANCEEXITBLOCK_WIDTH, VICUS::ENTRANCEEXITBLOCK_HEIGHT);
					block.m_mode = VICUS::BMBlockType::GlobalOutlet;
					block.m_displayName.clear();
					VICUS::BMSocket inlet;
					inlet.m_name = VICUS::INLET_NAME;
					inlet.m_isInlet = true;
					inlet.m_id = VICUS::EXIT_ID;
					inlet.m_orientation = Qt::Horizontal;
					inlet.m_pos = QPointF(0, VICUS::ENTRANCEEXITBLOCK_HEIGHT / 2);
					block.m_sockets.append(inlet);
				}

			}
		}
		m_sceneManager->updateNetwork(m_subNetwork->m_graphicalNetwork);

	} else {
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
	/* checks if every block has atleast one inlet and one outlet connection, if not,
	throw warning and decline to close the window */
	for(auto& block : m_sceneManager->network().m_blocks){
		if(block.m_mode == VICUS::BMBlockType::NetworkComponentBlock){
			if(!(m_sceneManager->isConnectedSocket(&block, &(block.m_sockets[0])) && m_sceneManager->isConnectedSocket(&block, &(block.m_sockets[1])))){
				QMessageBox::warning(this, tr("Warning"), tr("Not all Blocks are properly connected"));
				return false;
			}
		}
	}

	// Existing block connectivity setup
	std::map<int, std::vector<int>> connections;
	std::map<int, std::vector<int>> reverseConnections;
	for(auto& block : m_sceneManager->network().m_blocks){
		if(block.m_mode == VICUS::BMBlockType::GlobalInlet || block.m_mode == VICUS::BMBlockType::GlobalOutlet || block.m_mode == VICUS::BMBlockType::ConnectorBlock) continue;
		int inletID = block.m_sockets[0].m_id;
		int outletID = block.m_sockets[1].m_id;
		connections[inletID].push_back(outletID);
		reverseConnections[outletID].push_back(inletID);  // Reverse mapping for outlet traversal
	}

	// Traversal from global inlet
	std::set<int> visitedFromInlet;
	std::queue<int> queue;
	queue.push(VICUS::ENTRANCE_ID);
	while(!queue.empty()){
		int node = queue.front();
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
	std::set<int> visitedFromOutlet;
	queue.push(VICUS::EXIT_ID);
	while(!queue.empty()){
		int node = queue.front();
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
		if(block.m_mode == VICUS::BMBlockType::GlobalInlet || block.m_mode == VICUS::BMBlockType::GlobalOutlet|| block.m_mode == VICUS::BMBlockType::ConnectorBlock) continue;
		int inletID = block.m_sockets[0].m_id;
		int outletID = block.m_sockets[1].m_id;

		if (visitedFromInlet.find(inletID) == visitedFromInlet.end() || visitedFromOutlet.find(outletID) == visitedFromOutlet.end()) {
			// Block is not fully reachable
			allReachable = false;
			break;
		}
	}

	if(!allReachable) {
		QMessageBox::warning(this, tr("Warning"), tr("Not all Blocks are reachable from globalInlet and globalOutlet"));
		return false;
	}

	return true;
}

void SVSubNetworkEditDialog::on_buttonBox_accepted()
{
	if(!checkAcceptedNetwork()) return;

	m_subNetwork->m_graphicalNetwork = m_sceneManager->network();
	m_subNetwork->m_elements.clear();

	/* checks if there are any unused Components, if yes, delete them */
	for(unsigned int i = 0; i < m_subNetwork->m_components.size(); i++){
		bool toDelete = true;
		for(auto& block : m_subNetwork->m_graphicalNetwork.m_blocks){
			if(m_subNetwork->m_components[i].m_id == block.m_componentId){
				toDelete = false;
				break;
			}
		}
		if(toDelete){
			m_subNetwork->m_components.erase(m_subNetwork->m_components.begin() + i);
			i--;
		}
	}

	/* creates the appropriate Network Elements for the Subnetwork */
	for(auto& block : m_subNetwork->m_graphicalNetwork.m_blocks){
		if(block.m_mode == VICUS::BMBlockType::NetworkComponentBlock){
			VICUS::NetworkElement networkElement(block.m_name.toInt(), block.m_sockets[1].m_id, block.m_sockets[0].m_id, block.m_componentId);
			networkElement.m_controlElementId = block.m_controllerID;
			networkElement.m_displayName = block.m_displayName;
			networkElement.m_inletNodeId = block.m_sockets[0].m_id;
			networkElement.m_outletNodeId = block.m_sockets[1].m_id;
			m_subNetwork->m_elements.push_back(networkElement);
		}
	}

	/* fills subnetwork with Components and Controllers */
	m_subNetwork->m_components = m_networkComponents;
	m_subNetwork->m_controllers = m_networkControllers;

	this->close();
}

void SVSubNetworkEditDialog::on_buttonBox_rejected()
{
	this->close();
}

SVBMZoomMeshGraphicsView *SVSubNetworkEditDialog::zoomMeshGraphicsView(){
	return ui->viewWidget;
}


void SVSubNetworkEditDialog::blockSelectedEvent()
{
	ui->controllerLineEdit->clear();
	bool ok = false;
	VICUS::BMBlock  *blockToDisplay = nullptr;
	// retrieve list of selected Blocks
	QList<const VICUS::BMBlock*> blocks = m_sceneManager->selectedBlocks();
	// only fill in right widget if exactly one block is selected
	if(blocks.size() == 1){
		blockToDisplay = const_cast<VICUS::BMBlock*>(blocks.first());
		// if Block is a networkComponentBlock, fill data in the right widget
		if(blockToDisplay->m_mode == VICUS::BMBlockType::NetworkComponentBlock){
			//activate ControllerGroupBox
			ui->controllerGroupBox->setEnabled(true);
			ui->controllerLabel->setEnabled(true);
			ui->controllerLineEdit->setEnabled(true);
			ui->nameLineEdit->setEnabled(true);
			ui->nameLabel->setEnabled(true);
			ui->removeControllerButton->setEnabled(true);
			ui->removeBlockOrConnectorButton->setEnabled(true);
			ui->copyBlockButton->setEnabled(true);
			ui->addToDBButton->setEnabled(true);
			ui->nameLineEdit->setText(blockToDisplay->m_displayName);

			std::vector<NANDRAD::HydraulicNetworkControlElement::ControlledProperty> availableCtrProps;
			NANDRAD::HydraulicNetworkComponent::ModelType nandradModelType;
			nandradModelType = VICUS::NetworkComponent::nandradNetworkComponentModelType(m_networkComponents[componentIndex(blockToDisplay->m_componentId)].m_modelType);
			availableCtrProps = NANDRAD::HydraulicNetworkControlElement::availableControlledProperties(nandradModelType);
			if(availableCtrProps.size() > 0){
							ui->openControllerWidgetButton->setEnabled(true);
			} else {
							if(blockToDisplay->m_componentId != VICUS::INVALID_ID){
								ui->openControllerWidgetButton->setEnabled(true);
							}
							ui->openControllerWidgetButton->setEnabled(false);
			}

			if(blockToDisplay->m_controllerID != VICUS::INVALID_ID){
				const VICUS::NetworkController *ctr = VICUS::element(m_networkControllers, blockToDisplay->m_controllerID);
				ui->controllerLineEdit->setText(QString::fromLatin1(VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", ctr->m_controlledProperty)));
				ui->removeControllerButton->setEnabled(true);
			}
			else{
				ui->controllerLineEdit->setText(QString(" -"));
				ui->removeControllerButton->setEnabled(false);
			}

			// Fill NetworkComponent ComboBox
			int index = componentIndex(blockToDisplay->m_componentId);
			ui->networkComponentEditWidget->updateInput(index);

		} else if(blockToDisplay->m_mode == VICUS::BMBlockType::ConnectorBlock){
			ui->controllerGroupBox->setEnabled(true);
			ui->controllerLineEdit->setDisabled(true);
			ui->controllerLabel->setDisabled(true);
			ui->nameLineEdit->setDisabled(true);
			ui->nameLabel->setDisabled(true);
			ui->removeControllerButton->setDisabled(true);
			ui->openControllerWidgetButton->setDisabled(true);
			ui->networkComponentEditWidget->updateInput(-1);
			ui->removeBlockOrConnectorButton->setEnabled(true);
			ui->copyBlockButton->setEnabled(true);
			ui->addToDBButton->setEnabled(false);
		} else {
			selectionClearedEvent();
		}
	} else {
		selectionClearedEvent();
	}
}

void SVSubNetworkEditDialog::connectorSelectedEvent(const QString & sourceSocketName, const QString & targetSocketName)
{
	// if a connector is selected, check if it is connected to a ConnectorBlock, if not, enable deletion of connector
	if(sourceSocketName.contains(VICUS::CONNECTORBLOCK_NAME) || targetSocketName.contains(VICUS::CONNECTORBLOCK_NAME)){
		ui->removeBlockOrConnectorButton->setEnabled(false);
	} else{
		ui->removeBlockOrConnectorButton->setEnabled(true);
	}
}

void SVSubNetworkEditDialog::selectionClearedEvent(){
	ui->controllerLineEdit->clear();
	ui->controllerLineEdit->setDisabled(true);
	ui->controllerGroupBox->setDisabled(true);
	ui->nameLineEdit->setDisabled(true);
	ui->removeControllerButton->setDisabled(true);
	ui->openControllerWidgetButton->setDisabled(true);
	ui->networkComponentEditWidget->updateInput(-1);
	ui->removeBlockOrConnectorButton->setDisabled(true);
	ui->copyBlockButton->setDisabled(true);
	ui->addToDBButton->setEnabled(false);
	ui->nameLineEdit->clear();
}


void SVSubNetworkEditDialog::on_controllerDialog_accepted(VICUS::BMBlock *block, VICUS::NetworkController controller)
{
	if(block->m_controllerID == VICUS::INVALID_ID){
		block->m_controllerID = controller.m_id = getNewControllerID();
		m_networkControllers.push_back(controller);
	} else {
		m_networkControllers[controllerIndex(block->m_controllerID)] = controller;
	}
	ui->controllerLineEdit->setText(VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", static_cast<int>(controller.m_controlledProperty)));
	m_sceneManager->setControllerID(block, block->m_controllerID, VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", controller.m_controlledProperty));
	m_sceneManager->update();
}

void SVSubNetworkEditDialog::on_NameTextChanged(const QString& text){
	qDebug() << "Changed Name to " << text;
	if(m_sceneManager->selectedBlocks().size() == 0) return;
	VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().takeFirst());
	selectedBlock->m_displayName = text;
	m_sceneManager->update();
}

void SVSubNetworkEditDialog::on_newBlockAdded(VICUS::BMBlock *block, unsigned int componentID){
	if(componentID == VICUS::INVALID_ID){
		int newComponentID = createNewComponentID();
		m_networkComponents.push_back(VICUS::NetworkComponent());
		m_networkComponents.back().m_modelType = static_cast<VICUS::NetworkComponent::ModelType>(block->m_componentId);
		QString name = QString("New ") + VICUS::KeywordListQt::Keyword("NetworkComponent::ModelType", m_networkComponents.back().m_modelType);
		m_networkComponents.back().m_displayName = IBK::MultiLanguageString("en:" + name.toStdString());
		block->m_componentId = newComponentID;
		m_networkComponents.back().m_id = newComponentID;
	} else {
		auto networkComponents = m_db->m_networkComponents.begin();
		do{
			if(networkComponents->second.m_id == (unsigned int)componentID){
				VICUS::NetworkComponent component = networkComponents->second;
				component.m_builtIn = false;
				m_networkComponents.push_back(component);
				block->m_componentId = createNewComponentID();
				m_networkComponents.back().m_id = block->m_componentId;
				break;
			}
			networkComponents++;
		}while(networkComponents != m_db->m_networkComponents.end());
	}
}

unsigned int SVSubNetworkEditDialog::componentIndex(unsigned int componentID)
{
	for(unsigned int i = 0; i < m_networkComponents.size(); i++){
		if(m_networkComponents[i].m_id == componentID){
			return i;
		}
	}
	return -1;
}

unsigned int SVSubNetworkEditDialog::createNewComponentID()
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
	return VICUS::INVALID_ID;
}

unsigned int SVSubNetworkEditDialog::getNewControllerID(){
	unsigned int id = 0;
	for( unsigned int i = 0; i < m_networkControllers.size(); i++){
		if(m_networkControllers[i].m_id >= id){
			id = m_networkControllers[i].m_id + 1;
		}
	}
	return id;
}


void SVSubNetworkEditDialog::on_removeBlockOrConnectorButton_clicked()
{
	if(m_sceneManager->selectedBlocks().size() > 0){
		VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().takeFirst());
		if(selectedBlock->m_mode == VICUS::BMBlockType::GlobalInlet || selectedBlock->m_mode == VICUS::BMBlockType::GlobalOutlet) return;
		else if(selectedBlock->m_mode == VICUS::BMBlockType::ConnectorBlock){
			m_sceneManager->removeConnectorBlock(selectedBlock);
			m_sceneManager->update();
			return;
		} else{
			if(selectedBlock->m_componentId != VICUS::INVALID_ID)
				m_networkComponents.erase(m_networkComponents.begin() + componentIndex(selectedBlock->m_componentId));
			if(selectedBlock->m_controllerID != VICUS::INVALID_ID)
				m_networkControllers.erase(m_networkControllers.begin() + controllerIndex(selectedBlock->m_controllerID));
			m_sceneManager->removeBlock(selectedBlock);
			m_sceneManager->update();
		}
	} else if(m_sceneManager->selectedConnector()){
		m_sceneManager->removeSelectedConnector();
	}
}


void SVSubNetworkEditDialog::on_openControllerWidgetButton_clicked()
{
	if(m_controllerEditWidget == nullptr){
		m_controllerEditWidget = new SVDBNetworkControllerEditWidget(this, m_db);
		connect(m_controllerEditWidget, &SVDBNetworkControllerEditWidget::controllerAccepted, this, &SVSubNetworkEditDialog::on_controllerDialog_accepted);
	}

	VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().takeFirst());
	Q_ASSERT(selectedBlock != nullptr);
	if(selectedBlock->m_controllerID == VICUS::INVALID_ID){
		VICUS::NetworkController *controller = new VICUS::NetworkController();
		m_controllerEditWidget->setup(selectedBlock, *controller, &(m_networkComponents[componentIndex(selectedBlock->m_componentId)]));
	} else {
		VICUS::NetworkComponent* component = &(m_networkComponents[componentIndex(selectedBlock->m_componentId)]);
		unsigned int idx = controllerIndex(selectedBlock->m_controllerID);
		m_controllerEditWidget->setup(selectedBlock, (m_networkControllers[idx]), component);
	}

	m_controllerEditWidget->show();
}


void SVSubNetworkEditDialog::on_removeControllerButton_clicked()
{
	VICUS::BMBlock* selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().takeFirst());
	Q_ASSERT(selectedBlock != nullptr);
	if(selectedBlock->m_controllerID == VICUS::INVALID_ID) return;
	m_networkControllers.erase(m_networkControllers.begin() + controllerIndex(selectedBlock->m_controllerID));
	ui->controllerLineEdit->setText(" -");
	m_sceneManager->setControllerID(selectedBlock, VICUS::INVALID_ID, "");
	m_sceneManager->update();
}

void SVSubNetworkEditDialog::on_copyBlockButton_clicked()
{
	if(m_sceneManager->selectedBlocks().size() == 0) return;
	VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().takeFirst());
	Q_ASSERT(selectedBlock != nullptr);
	if(selectedBlock->m_mode == VICUS::BMBlockType::GlobalInlet || selectedBlock->m_mode == VICUS::BMBlockType::GlobalOutlet || selectedBlock->m_mode == VICUS::BMBlockType::ConnectorBlock) return;
	else{
		VICUS::BMBlock newBlock = *selectedBlock;
		newBlock.m_pos += QPointF(2*VICUS::BLOCK_WIDTH, 0);
		if(selectedBlock->m_controllerID != VICUS::INVALID_ID){
			VICUS::NetworkController controller = m_networkControllers[controllerIndex(selectedBlock->m_controllerID)];
			newBlock.m_controllerID = controller.m_id = getNewControllerID();
			controller.m_builtIn = false;
			m_networkControllers.push_back(controller);
		}

		VICUS::NetworkComponent component = m_networkComponents[componentIndex(selectedBlock->m_componentId)];
		newBlock.m_componentId = component.m_id = createNewComponentID();
		m_networkComponents.push_back(component);
		m_sceneManager->addBlock(newBlock);

		if(selectedBlock->m_controllerID != VICUS::INVALID_ID){
			for(auto &blockItr : m_sceneManager->network().m_blocks){
				if(blockItr.m_controllerID == newBlock.m_controllerID){
					const VICUS::NetworkController *ctr = VICUS::element(m_networkControllers, blockItr.m_controllerID);
					m_sceneManager->setControllerID(const_cast<VICUS::BMBlock*>(&blockItr), newBlock.m_controllerID,
																VICUS::KeywordListQt::Keyword("NetworkController::ControlledProperty", ctr->m_controlledProperty));
				}
			}
		}
		m_sceneManager->update();
	}
}

void SVSubNetworkEditDialog::on_componentSelected()
{
	m_senderTable = qobject_cast<SVSubNetworkEditDialogTable*>(sender());
	if(m_senderTable == nullptr) return;

	int row = m_senderTable->currentRow();

	if(row == -1){
		ui->removeFromUserDBButton->setEnabled(false);
		return;
	}
	if(m_senderTable->m_elementList[row].contains("db")){
		int index = m_senderTable->m_elementList[row].split(":")[2].toInt();
		ui->removeFromUserDBButton->setEnabled(!m_db->m_networkComponents[index]->m_builtIn);
	} else {
		ui->removeFromUserDBButton->setEnabled(false);
	}
}


void SVSubNetworkEditDialog::on_addToDBButton_clicked()
{
	if(m_sceneManager->selectedBlocks().size() == 0) return;
	VICUS::BMBlock *selectedBlock = const_cast<VICUS::BMBlock*>(m_sceneManager->selectedBlocks().takeFirst());
	Q_ASSERT(selectedBlock != nullptr);
	if(selectedBlock->m_mode == VICUS::BMBlockType::GlobalInlet || selectedBlock->m_mode == VICUS::BMBlockType::GlobalOutlet || selectedBlock->m_mode == VICUS::BMBlockType::ConnectorBlock) return;
	VICUS::NetworkComponent component = m_networkComponents[componentIndex(selectedBlock->m_componentId)];
	qDebug() << m_db->m_networkComponents.add(component) << "new ID";
	createToolBox();
}


void SVSubNetworkEditDialog::on_removeFromUserDBButton_clicked()
{
	if(m_senderTable == nullptr) return;
	int row = m_senderTable->currentRow();
	if(row == -1) return;
	unsigned int idToDelete = m_senderTable->m_elementList[row].split(":")[2].toInt();
	m_senderTable->clearSelection();
	m_db->m_networkComponents.remove(idToDelete);
	createToolBox();
}

