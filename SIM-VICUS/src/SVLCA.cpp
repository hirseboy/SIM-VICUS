#include "SVLCA.h"

#include "SVSettings.h"
#include <VICUS_EPDCategroySet.h>

namespace SV {

//check if element exists with ID
template <typename T>
T elementExists(std::map<unsigned int, T> database, unsigned int id, std::string displayName,
				std::string idName, std::string objectType){
	FUNCID(LCA::elementExists);
	if(database.find(id) == database.end())
		IBK::Exception(IBK::FormatString("%3 id %1 of %4 '%2' is not contained in database. ")
					   .arg(id).arg(displayName).arg(idName).arg(objectType), FUNC_ID);
	return  database[id];
}


/*! /// TODO Mira */
void addEpdMaterialToComponent(VICUS::EPDDataset epd, LCA::LCAComponentResult &comp, LCA::LCAComponentResult &comp2,
					double lifeCycle, double thickness, double rho, int idx = 0, double adjustment = 1.2){

	FUNCID(LCA::addEpdMaterialToComponent);

	for(unsigned int i=0; i<VICUS::EPDDataset::NUM_P; ++i){
		//skip empty values
		if(epd.m_para[i].value == 0)
			continue;

		IBK::Unit unit = epd.m_para[i].unit();
		//mass material
		double mass = rho * comp.m_area  * thickness;

		//take specific epd value for this category
		double val = epd.m_para[i].get_value(unit);
		QString &refUnit = epd.m_referenceUnit;
		val *= mass;
		if(QString::compare("kg", refUnit, Qt::CaseInsensitive) == 0){
			//do nothing
		}
		else if (QString::compare("m3", refUnit, Qt::CaseInsensitive) == 0) {
			//referencequantity in [m3]
			if(epd.m_para[VICUS::EPDDataset::P_Density].get_value("kg/m3") > 0)
				val /= epd.m_para[VICUS::EPDDataset::P_Density].get_value("kg/m3");		/// TODO Mira		/*volumen density*/
		}
		else if (QString::compare("m2", refUnit, Qt::CaseInsensitive) == 0) {
			//referencequantity in [m2]
			val /= epd.m_para[VICUS::EPDDataset::P_Density].get_value("kg/m3");	/// TODO Mira
		}
		else
			throw IBK::Exception(IBK::FormatString("No valid specific Unit???? rho for epd mat. '%1' available for").
								 arg(epd.m_displayName.toStdString()), FUNC_ID);
		val /= epd.m_referenceQuantity;

		val *= adjustment;

		//Das könnten wir auch später machen - wir überlegen hier nochmal
		/// TODO Mira
		//val /= netFloorArea;
		//val /= 50;					//reporting period [a]

		comp.addValue(idx, static_cast<VICUS::EPDDataset::para_t>(i),
					  IBK::Parameter(epd.m_para[i].name, val, unit));
		if(lifeCycle >= 0)
			comp2.addValue(idx, static_cast<VICUS::EPDDataset::para_t>(i),
						   IBK::Parameter(epd.m_para[i].name, val*lifeCycle, unit));
	}
}



void LCA::calculateLCA()
{
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

				//get component
				VICUS::Component comp = elementExists<VICUS::Component>(m_dbComponents, s.m_componentId,
												s.m_displayName.toStdString(),"Component", "surface");
				//save surface area
				compRes[comp.m_id].m_area += surf.m_geometry.area();
			}
		}
	}

	//calculate all lca for each component
	for (auto &c : compRes) {
		const VICUS::Component &comp = m_dbComponents[c.first];

		//opaque construction
		if(comp.m_idOpaqueConstruction != VICUS::INVALID_ID){
			//get construction
			VICUS::Construction constr =
					elementExists<VICUS::Construction>(m_dbConstructions, comp.m_idOpaqueConstruction,
													   comp.m_displayName.toStdString(),"Construction",
													   "component");

			//calc each construction
			for(auto l : constr.m_materialLayers){
				//double thickness = l.m_thickness.get_value("m");	//thickness in m
				//check if material exists
				VICUS::Material mat =
						elementExists<VICUS::Material>(m_dbOpaqueMaterials, l.m_matId,
														   constr.m_displayName.toStdString(),
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
																   mat.m_displayName.toStdString(),
																   "EPD",
																   "material");

					//if we found the right dataset add values A1- A2
					if(epd.m_category == VICUS::EPDDataset::C_A1 ||
					   epd.m_category == VICUS::EPDDataset::C_A2 ||
					   epd.m_category == VICUS::EPDDataset::C_A1_A2||
					   epd.m_category == VICUS::EPDDataset::C_A3 ||
					   epd.m_category == VICUS::EPDDataset::C_A1_A3){
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
					else if (epd.m_category == VICUS::EPDDataset::C_B6) {
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
					else if (epd.m_category == VICUS::EPDDataset::C_C2 ||
							 epd.m_category == VICUS::EPDDataset::C_C2_C4 ||
							 epd.m_category == VICUS::EPDDataset::C_C3 ||
							 epd.m_category == VICUS::EPDDataset::C_C2_3 ||
							 epd.m_category == VICUS::EPDDataset::C_C3_C4 ||
							 epd.m_category == VICUS::EPDDataset::C_C4) {
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
					else if (epd.m_category == VICUS::EPDDataset::C_D) {
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
		if(m_dbComponents[compId].m_idOpaqueConstruction != VICUS::INVALID_ID){
			const VICUS::Construction &constr = m_dbConstructions[m_dbComponents[compId].m_idOpaqueConstruction];

			//gehe durch alle material layer ///TODO Mira
			for(auto &l : constr.m_materialLayers){
				MatEpd &matEpd = materialIdAndEpd[l.m_matId];
				double rho = m_dbOpaqueMaterials[l.m_matId].m_para[VICUS::Material::P_Density].get_value("kg/m3");

				addEpdMaterialToComponent(matEpd.m_epdA, comp, compResErsatz[compId],
							   l.m_lifeCylce, l.m_thickness.get_value("m"),
							   rho, 0, m_adjustment);

				addEpdMaterialToComponent(matEpd.m_epdB, comp, compResErsatz[compId],
							   0, l.m_thickness.get_value("m"),
							   rho, 1, m_adjustment);

				addEpdMaterialToComponent(matEpd.m_epdC, comp, compResErsatz[compId],
							   l.m_lifeCylce, l.m_thickness.get_value("m"),
							   rho, 2, m_adjustment);

				addEpdMaterialToComponent(matEpd.m_epdD, comp, compResErsatz[compId],
							   l.m_lifeCylce, l.m_thickness.get_value("m"),
							   rho, 3, m_adjustment);

			}

		}

	}
}

}
