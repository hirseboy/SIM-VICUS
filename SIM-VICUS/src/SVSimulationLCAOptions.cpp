#include "SVSimulationLCAOptions.h"
#include "ui_SVSimulationLCAOptions.h"

#include <IBK_Parameter.h>
#include <IBK_FileReader.h>
#include <IBK_StopWatch.h>

#include <SVSettings.h>
#include <SVDatabase.h>
#include <SVDatabaseEditDialog.h>
#include <SVDBEpdTableModel.h>
#include <SVProjectHandler.h>

#include <VICUS_Project.h>
#include <VICUS_EpdDataset.h>
#include <VICUS_EpdCategoryDataset.h>

#include <QtExt_Conversions.h>

#include <QProgressDialog>

#include <fstream>

SVSimulationLCAOptions::SVSimulationLCAOptions(QWidget *parent, VICUS::LCASettings & settings) :
	QWidget(parent),
	m_ui(new Ui::SVSimulationLCAOptions),
	m_lcaSettings(&settings),
	m_prj(SVProjectHandler::instance().project())
{
	m_ui->setupUi(this);
	layout()->setMargin(0);

	//	m_lcaSettings->initDefaults();

	m_ui->lineEditTimePeriod->setText(QString("%1").arg(m_lcaSettings->m_para[VICUS::LCASettings::P_TimePeriod].get_value("a")));
	m_ui->lineEditPriceIncrease->setText(QString("%1").arg(m_lcaSettings->m_para[VICUS::LCASettings::P_PriceIncrease].get_value("%")));

	m_ui->filepathOekoBauDat->setup(tr("Select csv with ÖKOBAUDAT"), true, true, tr("ÖKOBAUDAT-csv (*.csv)"),
									SVSettings::instance().m_dontUseNativeDialogs);

	m_ui->filepathResults->setup("Select directory for LCA results", false, true, "", SVSettings::instance().m_dontUseNativeDialogs);

	m_db = &SVSettings::instance().m_db;
}

SVSimulationLCAOptions::~SVSimulationLCAOptions() {
	delete m_ui;
}

void SVSimulationLCAOptions::calculateLCA() {
	FUNCID(SVSimulationLCAOptions::calculateLCA);

	IBK::Path path(m_ui->filepathResults->filename().toStdString());
	QString filename = m_ui->lineEditResultName->text();
	IBK::Path file(filename.toStdString());
	file.addExtension(".txt");
	path /= file;

	if(!path.isValid()) {
		QMessageBox::warning(this, tr("Invalid Result path or file"), tr("Define a valid result path and filename first before calculating LCA."));
		return;
	}


	/// 1) Aggregate all used components from project and sum up all their areas
	/// 2) go through all layers and their referenced epds and use the epds reference unit for global calculation
	/// 3) Global calculation means that layer data needs to be converted in a manner that it coresponds to reference unit
	/// 4) Now calculate the final LCA Data for each component and material layer using converted material data and reference unit
	/// 5) Aggregate data for each component type --> such as floor, etc.

	// Reset all LCA Data from previous calculations.
	resetLcaData();

	// First we aggregate all data for used components
	aggregateProjectComponents();

	// Calculate all total EPD Data for Components
	calculateTotalLcaDataForComponents();

	// Aggregate all data by type of component
	aggregateAggregatedComponentsByType();

	// Write calculation to file
	writeLcaDataToTxtFile(path);


#if 0
	FUNCID(LCA::calculateLCA);

	/*! Summarize all components with all constructions and material layers.
		Categorize all construction with their surface areas.
	*/

	/* Annahmen: diese Strukturen müssen umbenannt werden. */
	std::map<unsigned int, VICUS::Component>				m_dbComponents;
	std::map<unsigned int, VICUS::Construction>				m_dbConstructions;
	std::map<unsigned int, VICUS::Material>					m_dbOpaqueMaterials;
	std::map<unsigned int, VICUS::EPDDataset>				m_dbEPDs;


	struct MatEpd{
		VICUS::EPDDataset m_epdA;
		VICUS::EPDDataset m_epdB;
		VICUS::EPDDataset m_epdC;
		VICUS::EPDDataset m_epdD;
	};

	std::map<unsigned int, LCAComponentResult>		compRes;
	std::map<unsigned int, LCAComponentResult>		compResErsatz;

	//holds the data for each material
	std::map<unsigned int, MatEpd>					materialIdAndEpd;
	double netFloorArea = m_building.m_netFloorArea;

	/* Calculate all surface areas according to all components. */
	for (auto &bl : m_building.m_buildingLevels) {
		for (auto &r : bl.m_rooms) {
			for (auto &s : r.m_surfaces) {
				const VICUS::Surface &surf = s;

				// get component
				const VICUS::ComponentInstance * compInstance = s.m_componentInstance;
				if (compInstance != nullptr) {
					VICUS::Component comp = elementExists<VICUS::Component>(m_dbComponents, compInstance->m_idComponent,
																			s.m_displayName.toStdString(),"Component", "surface");
					//save surface area
					compRes[comp.m_id].m_area += surf.geometry().area();
				}
				else {
					/// TODO : error handling if component instance pointer is empty (no component associated)
				}
			}
		}
	}

	//calculate all lca for each component
	for (auto &c : compRes) {
		const VICUS::Component &comp = m_dbComponents[c.first];

		//opaque construction
		if(comp.m_idConstruction != VICUS::INVALID_ID){
			//get construction
			///TODO Dirk baufähig gemacht müsste rückgängig gemacht werden
			VICUS::Construction constr;
			//					elementExists<VICUS::Construction>(m_dbConstructions, comp.m_idOpaqueConstruction,
			//													   comp.m_displayName.toStdString(),"Construction",
			//													   "component");

			//calculate each construction
			for(auto l : constr.m_materialLayers){
				//check if material exists
				VICUS::Material mat =
						elementExists<VICUS::Material>(m_dbOpaqueMaterials, l.m_idMaterial,
													   constr.m_displayName.string(),
													   "Material",
													   "construction");

				//material exists already in the new user database
				if(materialIdAndEpd.find(mat.m_id) != materialIdAndEpd.end())
					continue;

				MatEpd &matEpd = materialIdAndEpd[mat.m_id];
				//check each material epd id
				for (auto idEpd : mat.m_idEpds) {
					if(idEpd == VICUS::INVALID_ID)
						continue;

					VICUS::EPDDataset epd = elementExists<VICUS::EPDDataset>(m_dbEPDs, idEpd,
																			 mat.m_displayName.string(),
																			 "EPD",
																			 "material");

					//if we found the right dataset add values A1- A2
					if(epd.m_module == VICUS::EPDDataset::M_A1 ||
							epd.m_module == VICUS::EPDDataset::M_A2 ||
							epd.m_module == VICUS::EPDDataset::M_A1_A2||
							epd.m_module == VICUS::EPDDataset::M_A3 ||
							epd.m_module == VICUS::EPDDataset::M_A1_A3){
						//add all values in a category A
						for (unsigned int i=0;i< VICUS::EPDDataset::NUM_P; ++i) {
							IBK::Parameter para = epd.m_para[i];
							//...
							if(para.value != 0){
								matEpd.m_epdA.m_para[i].set(para.name,
															matEpd.m_epdA.m_para[i].get_value(para.unit())
															+ para.get_value(para.unit()),
															para.unit());
							}
						}
					}
					else if (epd.m_module == VICUS::EPDDataset::M_B6) {
						//add all values in a category B
						for (unsigned int i=0;i< VICUS::EPDDataset::NUM_P; ++i) {
							IBK::Parameter para = epd.m_para[i];
							//...
							if(para.value != 0){
								matEpd.m_epdB.m_para[i].set(para.name,
															matEpd.m_epdB.m_para[i].get_value(para.unit())
															+ para.get_value(para.unit()),
															para.unit());
							}
						}
					}
					else if (epd.m_module == VICUS::EPDDataset::M_C2 ||
							 epd.m_module == VICUS::EPDDataset::M_C2_C4 ||
							 epd.m_module == VICUS::EPDDataset::M_C3 ||
							 epd.m_module == VICUS::EPDDataset::M_C2_C3 ||
							 epd.m_module == VICUS::EPDDataset::M_C3_C4 ||
							 epd.m_module == VICUS::EPDDataset::M_C4) {
						//add all values in a category C
						for (unsigned int i=0;i< VICUS::EPDDataset::NUM_P; ++i) {
							IBK::Parameter para = epd.m_para[i];
							//...
							if(para.value != 0){
								matEpd.m_epdC.m_para[i].set(para.name,
															matEpd.m_epdC.m_para[i].get_value(para.unit())
															+ para.get_value(para.unit()),
															para.unit());
							}
						}
					}
					else if (epd.m_module == VICUS::EPDDataset::M_D) {
						//add all values in a category D
						for (unsigned int i=0;i< VICUS::EPDDataset::NUM_P; ++i) {
							IBK::Parameter para = epd.m_para[i];
							//...
							if(para.value != 0){
								matEpd.m_epdD.m_para[i].set(para.name,
															matEpd.m_epdD.m_para[i].get_value(para.unit())
															+ para.get_value(para.unit()),
															para.unit());
							}
						}
					}
				}
			}
		}
	}



	for (auto &e : compRes) {
		//Component result object
		LCAComponentResult &comp = e.second;
		unsigned int compId = e.first;

		//check if opaque construction is available
		if(m_dbComponents[compId].m_idConstruction != VICUS::INVALID_ID){
			const VICUS::Construction &constr = m_dbConstructions[m_dbComponents[compId].m_idConstruction];

			//get values for all material layers in each category of the lifecycle
			for(auto &l : constr.m_materialLayers){
				MatEpd &matEpd = materialIdAndEpd[l.m_idMaterial];
				double rho = m_dbOpaqueMaterials[l.m_idMaterial].m_para[VICUS::Material::P_Density].get_value("kg/m3");

				//				addEpdMaterialToComponent(matEpd.m_epdA, comp, compResErsatz[compId],
				//							   l.m_lifeCylce, l.m_thickness.get_value("m"),
				//							   rho, 0, m_adjustment);

				addEpdMaterialToComponent(matEpd.m_epdB, comp, compResErsatz[compId],
										  0, l.m_thickness.get_value("m"),
										  rho, 1, m_adjustment);

				//				addEpdMaterialToComponent(matEpd.m_epdC, comp, compResErsatz[compId],
				//							   l.m_lifeCylce, l.m_thickness.get_value("m"),
				//							   rho, 2, m_adjustment);

				//				addEpdMaterialToComponent(matEpd.m_epdD, comp, compResErsatz[compId],
				//							   l.m_lifeCylce, l.m_thickness.get_value("m"),
				//							   rho, 3, m_adjustment);

			}

		}

	}
#endif
}


bool convertString2Val(double &val, const std::string &text, unsigned int row, unsigned int column) {
	try {
		val = IBK::string2val<double>(text);
	}  catch (IBK::Exception &ex) {
		IBK::IBK_Message(IBK::FormatString("%4\nCould not convert string '%1' of row %2 and column %3")
						 .arg(text).arg(row+1).arg(column+1).arg(ex.what()));
		return false;
	}
	return true;
}


template<typename T>
void SVSimulationLCAOptions::setValue(T &member, const T &value, bool foundExistingEpd) {
	if(!foundExistingEpd)
		member = value;
	else {
		if(member != value) {
			qDebug() << "Error between already defined values of EPD '" << member << "' <> '" << value << "'";
		}
	}
}



void SVSimulationLCAOptions::importOkoebauDat(const IBK::Path & csvPath) {
	FUNCID(SVDBEPDEditWidget::importOkoebauDat);

	// Read csv with ÖKOBAUDAT
	std::vector< std::string > dataLines;

	if (SVSettings::instance().showDoNotShowAgainQuestion(this, "reloading-oekobaudat",
														  tr("Reloading ÖKOBAUDAT"),
														  tr("Reloading ÖKOBAUDAT will delete all references to currently existing EPDs in Database. Continue?"),
														  QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
		return;

	// Explode all lines
	if (IBK::FileReader::readAll(csvPath, dataLines, std::vector<std::string>()) == -1)
		throw IBK::Exception("Error reading csv-file with ÖKÖBAUDAT!", FUNC_ID);

	// Remove header
	dataLines.erase(dataLines.begin());

	// extract vector of string-xy-pairs
	std::vector<std::string> tokens;

	QProgressDialog *dlg = new QProgressDialog(tr("Importing EPD-Database"), tr("Stop"), 0, (int)dataLines.size(), this);

	std::map<QString, VICUS::EpdDataset> dataSets;

	IBK::StopWatch timer;
	timer.start();

	std::map<std::string, std::string> oekobauDatUnit2IBKUnit;
	oekobauDatUnit2IBKUnit["qm"] = "m2";
	oekobauDatUnit2IBKUnit["pcs."] = "-";
	oekobauDatUnit2IBKUnit["kgkm"] = "kg/m3";
	oekobauDatUnit2IBKUnit["MJ"] = "MJ";
	oekobauDatUnit2IBKUnit["m3"] = "m3";
	oekobauDatUnit2IBKUnit["m"] = "m";
	oekobauDatUnit2IBKUnit["kg"] = "kg";
	oekobauDatUnit2IBKUnit["a"] = "a";


	for (unsigned int row = 0; row<dataLines.size(); ++row) {
		std::string &line = dataLines[row];
		//		IBK::trim(line, ",");
		//		IBK::trim(line, "\"");
		//		IBK::trim(line, "MULTILINESTRING ((");
		//		IBK::trim(line, "))");
		IBK::explode(line, tokens, ";", IBK::EF_KeepEmptyTokens);

		VICUS::EpdDataset *epd = nullptr;
		VICUS::EpdCategoryDataset *epdCategoryDataSet = new VICUS::EpdCategoryDataset;

		// convert this vector to double and add it as a graph
		std::vector<std::vector<double> > polyLine;
		for (unsigned int col = 0; col < tokens.size(); ++col){

			bool foundExistingEpd = false;

			std::string t = IBK::ANSIToUTF8String(tokens[col]);

			if(timer.difference() > 200) {
				dlg->setValue(row);
				qApp->processEvents();
			}

			if(dlg->wasCanceled()) {
				IBK::IBK_Message(IBK::FormatString("EPD-Import has been interrupted by user."));
				return;
			}

			// qDebug() << "Row: " << row << " Column: " << col << " Text: " << QString::fromStdString(t);

			if (t == "" || t == "not available")
				continue;

			switch (col) {
				// Not imported coloumns
				case ColVersion:
				case ColConformity:
				case ColCountryCode:
				case ColReferenceYear:
				case ColPublishedOn:
				case ColRegistrationNumber:
				case ColRegistrationBody:
				case ColUUIDOfThePredecessor:
					break;

				case ColUUID: {

					QString uuid = QString::fromStdString(t);

					// do we already have an EPD with the specific UUID
					if(dataSets.find(uuid) != dataSets.end()) { // We found one
						epd = &dataSets[uuid];
						foundExistingEpd = true;
					}
					else { // Create new one
						epd = new VICUS::EpdDataset;
						dataSets[uuid] = *epd;
					}

					setValue<QString>(epd->m_uuid, uuid, foundExistingEpd);

				} break;
				case ColNameDe:				epd->m_displayName.setString(t, "De");			break;
				case ColNameEn:				epd->m_displayName.setString(t, "En");			break;
				case ColCategoryDe:			epd->m_category.setString(t, "De");				break;
				case ColCategoryEn:			epd->m_category.setString(t, "En");				break;
				case ColType: {
					VICUS::EpdDataset::Type type = VICUS::EpdDataset::NUM_T;

					if (t == "average dataset")
						type = VICUS::EpdDataset::T_Average;
					else if (t == "specific dataset")
						type = VICUS::EpdDataset::T_Specific;
					else if (t == "representative dataset")
						type = VICUS::EpdDataset::T_Representative;
					else if (t == "generic dataset")
						type = VICUS::EpdDataset::T_Generic;
					else if (t == "template dataset")
						type = VICUS::EpdDataset::T_Template;

					setValue<VICUS::EpdDataset::Type>(epd->m_type, type, foundExistingEpd);

				} break;
				case ColExpireYear:			epd->m_expireYear = QString::fromStdString(t);			break;
				case ColDeclarationOwner:	epd->m_manufacturer = QString::fromStdString(t);			break;
				case ColReferenceSize: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;


					setValue<double>(epd->m_referenceQuantity, val, foundExistingEpd);
				} break;
				case ColReferenceUnit: {

					if(!foundExistingEpd) {
						IBK::Unit unit(oekobauDatUnit2IBKUnit[t]);
						epd->m_referenceUnit = unit;
					}
					else {
						if(epd->m_referenceUnit.name() != oekobauDatUnit2IBKUnit[t])
							qDebug() << "Units do not match";
					}

				} break;
				case ColURL: {
					QString string = QString::fromStdString(t);
					setValue<QString>(epd->m_dataSource, string, foundExistingEpd);
				} break;


				case ColModule: {
					VICUS::EpdCategoryDataset::Module module = VICUS::EpdCategoryDataset::NUM_M;

					if (t == "A1")
						module = VICUS::EpdCategoryDataset::M_A1;
					else if (t == "A2")
						module = VICUS::EpdCategoryDataset::M_A2;
					else if (t == "A3")
						module = VICUS::EpdCategoryDataset::M_A3;
					else if (t == "A1-A2")
						module = VICUS::EpdCategoryDataset::M_A1_A2;
					else if (t == "A1-A3")
						module = VICUS::EpdCategoryDataset::M_A1_A3;
					else if (t == "A4")
						module = VICUS::EpdCategoryDataset::M_A4;
					else if (t == "A5")
						module = VICUS::EpdCategoryDataset::M_A5;
					else if (t == "B1")
						module = VICUS::EpdCategoryDataset::M_B1;
					else if (t == "B2")
						module = VICUS::EpdCategoryDataset::M_B2;
					else if (t == "B3")
						module = VICUS::EpdCategoryDataset::M_B3;
					else if (t == "B4")
						module = VICUS::EpdCategoryDataset::M_B4;
					else if (t == "B5")
						module = VICUS::EpdCategoryDataset::M_B5;
					else if (t == "B6")
						module = VICUS::EpdCategoryDataset::M_B6;
					else if (t == "B7")
						module = VICUS::EpdCategoryDataset::M_B7;
					else if (t == "C1")
						module = VICUS::EpdCategoryDataset::M_C1;
					else if (t == "C2")
						module = VICUS::EpdCategoryDataset::M_C2;
					else if (t == "C2-C3")
						module = VICUS::EpdCategoryDataset::M_C2_C3;
					else if (t == "C2-C4")
						module = VICUS::EpdCategoryDataset::M_C2_C4;
					else if (t == "C3")
						module = VICUS::EpdCategoryDataset::M_C3;
					else if (t == "C3-C4")
						module = VICUS::EpdCategoryDataset::M_C3_C4;
					else if (t == "C4")
						module = VICUS::EpdCategoryDataset::M_C4;
					else if (t == "D")
						module = VICUS::EpdCategoryDataset::M_D;

					epdCategoryDataSet->m_module = module;

				} break;


				case ColWeightPerUnitArea: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;

					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_AreaDensity].set("AreaDensity", val, IBK::Unit("kg/m2"));
				} break;
				case ColBulkDensity: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;
					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_DryDensity].set("DryDensity", val, IBK::Unit("kg/m3"));
				} break;
				case ColGWP: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;

					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_GWP].set("GWP", val, IBK::Unit("kg"));
				} break;
				case ColODP: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;
					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_ODP].set("ODP", val, IBK::Unit("kg"));
				} break;
				case ColPOCP: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;
					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_POCP].set("POCP", val, IBK::Unit("kg"));

				} break;
				case ColAP: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;
					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_AP].set("AP", val, IBK::Unit("kg"));
				} break;
				case ColEP: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;
					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_EP].set("EP", val, IBK::Unit("kg"));
				} break;
				case ColPENRT: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;
					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_PENRT].set("PENRT", val, IBK::Unit("W/mK"));
				} break;
				case ColPERT: {
					double val;
					if(!convertString2Val(val, t, row, col))
						continue;
					epdCategoryDataSet->m_para[VICUS::EpdCategoryDataset::P_PERT].set("PERT", val, IBK::Unit("W/mK"));
				} break;
			}
		}
		epd->m_epdCategoryDataset.push_back(*epdCategoryDataSet);
	}

	dlg->setValue(dataLines.size());
	// Get pointer to our EPD edit widget
	SVDatabaseEditDialog *conEditDialog = SVMainWindow::instance().dbEpdEditDialog();
	Q_ASSERT(conEditDialog != nullptr);

	// now we import all Datasets
	dynamic_cast<SVDBEpdTableModel*>(conEditDialog->dbModel())->importDatasets(dataSets);
}


void SVSimulationLCAOptions::addComponentInstance(const VICUS::ComponentInstance & compInstance) {
	if(m_compIdToAggregatedData.find(compInstance.m_idComponent) == m_compIdToAggregatedData.end())
		m_compIdToAggregatedData[compInstance.m_idComponent] = AggregatedComponentData(compInstance);
	else
		m_compIdToAggregatedData[compInstance.m_idComponent].addArea(compInstance);
}


void SVSimulationLCAOptions::aggregateProjectComponents() {
	QStringList lifetime, cost, epd;

	for(const VICUS::ComponentInstance &ci : m_prj.m_componentInstances) {
		// Add CI to aggregated data map
		addComponentInstance(ci);

		const VICUS::Component *comp = m_db->m_components[ci.m_idComponent];
		if(comp == nullptr)
			continue;

		const VICUS::Construction *con = m_db->m_constructions[comp->m_idConstruction];
		if(con == nullptr)
			continue;

		for(const VICUS::MaterialLayer &matLayer : con->m_materialLayers) {
			// Check Cost & lifetime of used Materials
			bool isLifetimeDefined = !matLayer.m_lifetime.empty();
			bool isCostDefined = !matLayer.m_cost.empty();
			// Check EPDs of used Materials
			const VICUS::Material *mat = m_db->m_materials[matLayer.m_idMaterial];

			if(mat == nullptr)
				continue;

			bool isEPDDefined = !mat->m_epdCategorySet.isEmpty();

			if(!isLifetimeDefined)
				lifetime << QString::fromStdString(mat->m_displayName.string());
			if(!isCostDefined)
				cost << QString::fromStdString(mat->m_displayName.string());
			if(!isEPDDefined) {
				epd << QString::fromStdString(mat->m_displayName.string());
				m_idComponentEpdUndefined.insert(comp->m_id);
			}
		}
	}

	lifetime.removeDuplicates();
	cost.removeDuplicates();
	epd.removeDuplicates();


	QString messageText(tr("Lifetime:\t%1\nCost:\t%2\nEPD:\t%3\n\nProceed and skip all components without needed Data?")
						.arg(lifetime.join("\n\t\t"))
						.arg(cost.join("\n\t\t"))
						.arg(epd.join("\n\t\t")));
	if(QMessageBox::warning(this, "LCA/LCC Information is missing", messageText) == QMessageBox::Cancel)
		return;
}

void SVSimulationLCAOptions::aggregateAggregatedComponentsByType() {
	for(std::map<unsigned int, AggregatedComponentData>::iterator itAggregatedComp = m_compIdToAggregatedData.begin();
		itAggregatedComp != m_compIdToAggregatedData.end(); ++itAggregatedComp)
	{
		const AggregatedComponentData &aggregatedData = itAggregatedComp->second;
		if(m_typeToAggregatedCompData.find(aggregatedData.m_component->m_type) == m_typeToAggregatedCompData.end())
			m_typeToAggregatedCompData[aggregatedData.m_component->m_type] = aggregatedData;
		else
			m_typeToAggregatedCompData[aggregatedData.m_component->m_type].addAggregatedData(aggregatedData);
	}
}

double SVSimulationLCAOptions::conversionFactorEpdReferenceUnit(const IBK::Unit & refUnit, const VICUS::Material &layerMat,
																double layerThickness, double layerArea){
	if(refUnit.name() == "kg")
		return layerArea * layerThickness * layerMat.m_para[VICUS::Material::P_Density].get_value("kg/m3"); // area * thickness * density --> layer mass

	if(refUnit.name() == "m2")
		return layerArea;

	if(refUnit.name() == "m3")
		return layerArea * layerThickness;

	if(refUnit.name() == "-")
		return 1; // Pieces are always set to 1 for now

	if(refUnit.name() == "MJ")
		return 1; // Also not implemented yet

	if(refUnit.name() == "kg/m3")
		return layerMat.m_para[VICUS::Material::P_Density].get_value("kg/m3");

	if(refUnit.name() == "a")
		return 50; // Also not implemented yet
}

void SVSimulationLCAOptions::writeDataToStream(std::ofstream &lcaStream, const std::string &categoryText,
											   const AggregatedComponentData::Category &category) {

	lcaStream << categoryText + "\t\t\t\t\t\t\t"  << std::endl;

	for(std::map<VICUS::Component::ComponentType, AggregatedComponentData>::iterator itAggregatedComp = m_typeToAggregatedCompData.begin();
		itAggregatedComp != m_typeToAggregatedCompData.end(); ++itAggregatedComp)
	{

		QStringList lcaDataType;
		const AggregatedComponentData &aggregatedTypeData = itAggregatedComp->second;

		std::set<unsigned int> usedCompIds;
		for(const VICUS::Component *comp : aggregatedTypeData.m_additionalComponents) {
			if(m_idComponentEpdUndefined.find(comp->m_id) == m_idComponentEpdUndefined.end()) {
				usedCompIds.insert(comp->m_id);
			}
		}
		if(usedCompIds.empty())
			continue;

		lcaDataType << "";
		lcaDataType << VICUS::KeywordList::Description("Component::ComponentType", aggregatedTypeData.m_component->m_type);
		lcaDataType << "";
		lcaDataType << QString::number(aggregatedTypeData.m_area);
//		lcaDataType << QString::number(aggregatedTypeData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_GWP].get_value());
//		lcaDataType << QString::number(aggregatedTypeData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_ODP].get_value());
//		lcaDataType << QString::number(aggregatedTypeData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_POCP].get_value());
//		lcaDataType << QString::number(aggregatedTypeData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_AP].get_value());
//		lcaDataType << QString::number(aggregatedTypeData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_EP].get_value());

		lcaStream << lcaDataType.join("\t").toStdString() << std::endl;

		for(const VICUS::Component *comp : aggregatedTypeData.m_additionalComponents) {

			const AggregatedComponentData &aggregatedCompData = m_compIdToAggregatedData[comp->m_id];

			if(usedCompIds.find(comp->m_id) == usedCompIds.end())
				continue; // Skip unused ids

			QStringList lcaData;

			lcaData << "";
			lcaData << "";
			lcaData << QtExt::MultiLangString2QString(comp->m_displayName);
			lcaData << QString::number(aggregatedCompData.m_area);
//			lcaData << QString::number(aggregatedCompData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_GWP].get_value());
//			lcaData << QString::number(aggregatedCompData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_ODP].get_value());
//			lcaData << QString::number(aggregatedCompData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_POCP].get_value());
//			lcaData << QString::number(aggregatedCompData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_AP].get_value());
//			lcaData << QString::number(aggregatedCompData.m_totalEpdData[category].m_para[VICUS::EpdDataset::P_EP].get_value());

			lcaStream << lcaData.join("\t").toStdString() << std::endl;

		}

	}
}

void SVSimulationLCAOptions::writeLcaDataToTxtFile(const IBK::Path &resultPath) {
	std::ofstream lcaStream(resultPath.str());
	// Write header
	lcaStream << "Category\tComponent type\tComponent name\tArea [m2]\tGWP (CO2-Äqu.) [kg/(m2a)\tODP (R11-Äqu.) [kg/(m2a)]\tPOCP (C2H4-Äqu.) [kg/(m2a)]\tAP (SO2-Äqu.) [kg/(m2a)]\tEP (PO4-Äqu.) [kg/(m2a)]" << std::endl;

	lcaStream << "Goal:\t\t\t\t24\t0.0000001010\t0.0063\t0.0662\t0.0086" << std::endl;


	writeDataToStream(lcaStream, "Category A - Production", AggregatedComponentData::C_CategoryA);
	// writeDataToStream(lcaStream, "Category A - Production", AggregatedComponentData::C_CategoryA);
	writeDataToStream(lcaStream, "Category C - Disposal", AggregatedComponentData::C_CategoryC);
	writeDataToStream(lcaStream, "Category D - Deposit", AggregatedComponentData::C_CategoryD);

	lcaStream.close();

}


void SVSimulationLCAOptions::calculateTotalLcaDataForComponents() {
	// Go through all used components.
	// Go through all used material layers in components.

	for(std::map<unsigned int, AggregatedComponentData>::iterator itAggregatedComp = m_compIdToAggregatedData.begin();
		itAggregatedComp != m_compIdToAggregatedData.end(); ++itAggregatedComp)
	{
		if(m_idComponentEpdUndefined.find(itAggregatedComp->first) != m_idComponentEpdUndefined.end())
			continue; // we skip components when there are epds not defined

		double area = itAggregatedComp->second.m_area;
		const VICUS::Component *comp = itAggregatedComp->second.m_component;

		qDebug() << QString::fromStdString(comp->m_displayName.string());
		qDebug() << comp->m_idConstruction;

		if(comp == nullptr)
			continue;

		const VICUS::Construction *con = m_db->m_constructions[comp->m_idConstruction];
		if(con == nullptr)
			continue;

		for(const VICUS::MaterialLayer &matLayer : con->m_materialLayers) {
			const VICUS::Material &mat = *m_db->m_materials[matLayer.m_idMaterial];
			const VICUS::EpdDataset *epdCatA = m_db->m_epdDatasets[mat.m_epdCategorySet.m_idCategoryA];
			const VICUS::EpdDataset *epdCatB = m_db->m_epdDatasets[mat.m_epdCategorySet.m_idCategoryB];
			const VICUS::EpdDataset *epdCatC = m_db->m_epdDatasets[mat.m_epdCategorySet.m_idCategoryC];
			const VICUS::EpdDataset *epdCatD = m_db->m_epdDatasets[mat.m_epdCategorySet.m_idCategoryD];


			// We do the unit conversion and handling to get all our reference units correctly managed
			if(epdCatA != nullptr)
				itAggregatedComp->second.m_totalEpdData[AggregatedComponentData::C_CategoryA]
						= epdCatA->scaleByFactor(
							conversionFactorEpdReferenceUnit(epdCatA->m_referenceUnit,
															 mat, matLayer.m_thickness.get_value("m"), area));
			if(epdCatB != nullptr)
				itAggregatedComp->second.m_totalEpdData[AggregatedComponentData::C_CategoryB]
						= epdCatB->scaleByFactor(
							conversionFactorEpdReferenceUnit(epdCatB->m_referenceUnit,
															 mat, matLayer.m_thickness.get_value("m"), area));
			if(epdCatC != nullptr)
				itAggregatedComp->second.m_totalEpdData[AggregatedComponentData::C_CategoryC]
						= epdCatC->scaleByFactor(
							conversionFactorEpdReferenceUnit(epdCatC->m_referenceUnit,
															 mat, matLayer.m_thickness.get_value("m"), area));
			if(epdCatD != nullptr)
				itAggregatedComp->second.m_totalEpdData[AggregatedComponentData::C_CategoryD]
						= epdCatD->scaleByFactor(
							conversionFactorEpdReferenceUnit(epdCatD->m_referenceUnit,
															 mat, matLayer.m_thickness.get_value("m"), area));

		}
	}
}


void SVSimulationLCAOptions::resetLcaData() {
	m_compIdToAggregatedData.clear();
	m_typeToAggregatedCompData.clear();
	m_idComponentEpdUndefined.clear();
}


void SVSimulationLCAOptions::on_pushButtonImportOkoebaudat_clicked() {
	FUNCID(SVDBEPDEditWidget::on_pushButtonImportOkoebaudat_clicked);

	IBK::Path path(m_ui->filepathOekoBauDat->filename().toStdString());

	if(!path.isValid()) {
		QMessageBox::warning(this, tr("Select ÖKOBAUDAT csv-file"), tr("Please select first a csv-file with valid ÖKOBAUDAT data."));
		return;
	}

	try {
		importOkoebauDat(path);
	}
	catch (IBK::Exception &ex) {
		throw IBK::Exception(IBK::FormatString("%1\nCould not import EPD-Database.").arg(ex.what()), FUNC_ID);
	}
}


void SVSimulationLCAOptions::on_pushButtonLcaLcc_clicked() {
	calculateLCA();
}

