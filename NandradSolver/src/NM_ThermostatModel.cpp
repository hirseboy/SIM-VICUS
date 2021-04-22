#include "NM_ThermostatModel.h"

#include <IBK_Exception.h>
#include <IBK_physics.h>

#include <NANDRAD_SimulationParameter.h>
#include <NANDRAD_Thermostat.h>
#include <NANDRAD_ObjectList.h>
#include <NANDRAD_Zone.h>

#include "NM_KeywordList.h"
#include "NM_Controller.h"

namespace NANDRAD_MODEL {



void ThermostatModel::setup(const NANDRAD::Thermostat & thermostat,
							const std::vector<NANDRAD::ObjectList> & objLists,
							const std::vector<NANDRAD::Zone> & zones)
{
	FUNCID(ThermostatModel::setup);

	m_thermostat = &thermostat;
	m_zones = &zones;

	// all models require an object list with indication of ventilated zones
	if (m_thermostat->m_zoneObjectList.empty())
		throw IBK::Exception(IBK::FormatString("Missing 'ZoneObjectList' parameter."), FUNC_ID);
	// check and resolve reference to object list
	std::vector<NANDRAD::ObjectList>::const_iterator oblst_it = std::find(objLists.begin(),
																		  objLists.end(),
																		  m_thermostat->m_zoneObjectList);
	if (oblst_it == objLists.end())
		throw IBK::Exception(IBK::FormatString("Invalid/undefined ZoneObjectList '%1'.")
							 .arg(m_thermostat->m_zoneObjectList), FUNC_ID);
	m_objectList = &(*oblst_it);
	// ensure correct reference type of object list
	if (m_objectList->m_referenceType != NANDRAD::ModelInputReference::MRT_ZONE)
		throw IBK::Exception(IBK::FormatString("Invalid reference type in object list '%1', expected type 'Zone'.")
							 .arg(m_objectList->m_name), FUNC_ID);

	// reserve storage memory for results
	m_vectorValuedResults.resize(NUM_VVR);

	// check if reference zone is given and check if such a zone exists
	if (m_thermostat->m_referenceZoneID != NANDRAD::INVALID_ID) {
		std::vector<NANDRAD::Zone>::const_iterator zone_it = std::find(zones.begin(), zones.end(),
			m_thermostat->m_referenceZoneID);
		if (zone_it == zones.end())
			throw IBK::Exception(IBK::FormatString("Invalid/undefined zone ID #%1.")
								 .arg(m_thermostat->m_referenceZoneID), FUNC_ID);
	}

	// initialize controller
	switch (m_thermostat->m_controllerType) {
		case NANDRAD::Thermostat::NUM_CT : // default to P-Controller
		case NANDRAD::Thermostat::CT_PController : {
			PController * con = new PController;
			con->m_kP = m_thermostat->m_para[NANDRAD::Thermostat::P_TemperatureTolerance].value;
			m_controller = con;
		} break;

		case NANDRAD::Thermostat::CT_DigitalController : {
			DigitalHysteresisController * con = new DigitalHysteresisController;
			con->m_hysteresisBand = m_thermostat->m_para[NANDRAD::Thermostat::P_TemperatureBand].value;
			m_controller = con;
		} break;

	}

	// the rest of the initialization can only be done when the object lists have been initialized, i.e. this happens in resultDescriptions()
}


void ThermostatModel::initResults(const std::vector<AbstractModel *> &) {
//	FUNCID(ThermostatModel::initResults);

	// no model IDs, nothing to do (see explanation in resultDescriptions())
	if (m_objectList->m_filterID.m_ids.empty())
		return; // nothing to compute, return
	// get IDs of ventilated zones
	std::vector<unsigned int> indexKeys(m_objectList->m_filterID.m_ids.begin(), m_objectList->m_filterID.m_ids.end());
	// resize result vectors accordingly
	for (unsigned int varIndex=0; varIndex<NUM_VVR; ++varIndex)
		m_vectorValuedResults[varIndex] = VectorValuedQuantity(indexKeys);
}


void ThermostatModel::resultDescriptions(std::vector<QuantityDescription> & resDesc) const {
	// during initialization of the object lists, only those zones were added, that are actually parameterized
	// so we can rely on the existence of zones whose IDs are in our object list and we do not need to search
	// through all the models

	// it may be possible, that an object list does not contain a valid id, for example, when the
	// requested IDs did not exist - in this case a warning was already printed, so we can just bail out here
	if (m_objectList->m_filterID.m_ids.empty())
		return; // nothing to compute, return

	// Retrieve index information from vector valued results.
	std::vector<unsigned int> indexKeys(m_objectList->m_filterID.m_ids.begin(), m_objectList->m_filterID.m_ids.end());

	// For each of the zones in the object list we generate vector-valued results as defined
	// in the type VectorValuedResults.
	for (int varIndex=0; varIndex<NUM_VVR; ++varIndex) {
		// store name, unit and description of the vector quantity
		const std::string &quantityName = KeywordList::Keyword("ThermostatModel::VectorValuedResults", varIndex );
		const std::string &quantityUnit = KeywordList::Unit("ThermostatModel::VectorValuedResults", varIndex );
		const std::string &quantityDescription = KeywordList::Description("ThermostatModel::VectorValuedResults", varIndex );
		// vector-valued quantity descriptions store the description
		// of the quantity itself as well as key strings and descriptions
		// for all vector elements
		resDesc.push_back( QuantityDescription(
			quantityName, quantityUnit, quantityDescription,
			false, VectorValuedQuantityIndex::IK_ModelID, indexKeys) );
	}
}


const double * ThermostatModel::resultValueRef(const InputReference & quantity) const {
	const QuantityName & quantityName = quantity.m_name;
	// determine variable enum index
	unsigned int varIndex=0;
	for (; varIndex<NUM_VVR; ++varIndex) {
		if (KeywordList::Keyword("ThermostatModel::VectorValuedResults", (VectorValuedResults)varIndex ) == quantityName.m_name)
			break;
	}
	if (varIndex == NUM_VVR)
		return nullptr;
	// now check the index
	if (quantityName.m_index == -1) // no index - return start of vector
		return m_vectorValuedResults[varIndex].dataPtr();
	// search for index
	try {
		const double & valRef = m_vectorValuedResults[varIndex][(unsigned int)quantityName.m_index];
		return &valRef;
	} catch (...) {
		// exception is thrown when index is not available - return nullptr
		return nullptr;
	}
}


void ThermostatModel::inputReferences(std::vector<InputReference> & inputRefs) const {
	if (m_objectList->m_filterID.m_ids.empty())
		return; // nothing to compute, return

	// distinguish between single-zone sensor and per-zone-sensor
	if (m_thermostat->m_referenceZoneID != NANDRAD::INVALID_ID) {
		// only one input reference to the selected zone
		InputReference ref;
		ref.m_id = m_thermostat->m_referenceZoneID;
		ref.m_referenceType = NANDRAD::ModelInputReference::MRT_ZONE;
		switch (m_thermostat->m_temperatureType) {
			case NANDRAD::Thermostat::NUM_TT: // default to air temperature
			case NANDRAD::Thermostat::TT_AirTemperature:
				ref.m_name.m_name = "AirTemperature";
			break;
			case NANDRAD::Thermostat::TT_OperativeTemperature:
				ref.m_name.m_name = "OperativeTemperature";
			break;
		}
		ref.m_required = true;
		inputRefs.push_back(ref);
		// scheduled model variant?
		if (m_thermostat->m_modelType == NANDRAD::Thermostat::MT_Scheduled) {
			// add references to heating and cooling setpoints
			// Note: for now we require these... better to be very strict than to be lazy and cause hard-to-detect
			//       problems when using the model.
			InputReference ref;
			ref.m_id = m_thermostat->m_referenceZoneID;
			ref.m_referenceType = NANDRAD::ModelInputReference::MRT_ZONE;
			ref.m_name.m_name = "HeatingSetpointSchedule";
			ref.m_required = true;
			inputRefs.push_back(ref);

			ref.m_name.m_name = "CoolingSetpointSchedule";
			inputRefs.push_back(ref);
		}
	}
	else {
		// each zone's thermstat uses the zone-specific inputs
		for (unsigned int id : m_objectList->m_filterID.m_ids) {
			InputReference ref;
			ref.m_id = id;
			ref.m_referenceType = NANDRAD::ModelInputReference::MRT_ZONE;
			switch (m_thermostat->m_temperatureType) {
				case NANDRAD::Thermostat::NUM_TT: // default to air temperature
				case NANDRAD::Thermostat::TT_AirTemperature:
					ref.m_name.m_name = "AirTemperature";
				break;
				case NANDRAD::Thermostat::TT_OperativeTemperature:
					ref.m_name.m_name = "OperativeTemperature";
				break;
			}
			ref.m_required = true;
			inputRefs.push_back(ref);
			// scheduled model variant?
			if (m_thermostat->m_modelType == NANDRAD::Thermostat::MT_Scheduled) {
				// add references to heating and cooling setpoints
				// Note: for now we require these... better to be very strict than to be lazy and cause hard-to-detect
				//       problems when using the model.
				ref.m_name.m_name = "HeatingSetpointSchedule";
				inputRefs.push_back(ref);

				ref.m_name.m_name = "CoolingSetpointSchedule";
				inputRefs.push_back(ref);
			}
		}
	}

}


void ThermostatModel::setInputValueRefs(const std::vector<QuantityDescription> & /*resultDescriptions*/,
												const std::vector<const double *> & resultValueRefs)
{
	// count number of inputs that we expect
	// zone temperatures
	unsigned int expectedSize =  m_objectList->m_filterID.m_ids.size(); // all zone's temperatures
	if (m_thermostat->m_referenceZoneID != NANDRAD::INVALID_ID)
		expectedSize = 1;
	// for thermostat with scheduled setpoints, we have a schedule input for each zone
	if (m_thermostat->m_modelType == NANDRAD::Thermostat::MT_Scheduled)
		expectedSize *= 3; // AirTemperature, HeatingSetpointSchedule and CoolingSetpointSchedule
	IBK_ASSERT(resultValueRefs.size() == expectedSize);
	m_valueRefs = resultValueRefs;
}


void ThermostatModel::stateDependencies(std::vector<std::pair<const double *, const double *> > & resultInputValueReferences) const {
	if (m_objectList->m_filterID.m_ids.empty())
		return; // nothing to compute, return
	// we compute ventilation rates per zone and heat fluxes per zone, ventilation rates (currently) have
	// no dependencies (only from schedules), but heat fluxes depend on ambient temperatures and on zone temperatures
	for (unsigned int i=0; i<m_objectList->m_filterID.m_ids.size(); ++i) {
		// pair: result - input

		// dependency on room air temperature of corresponding zone
		resultInputValueReferences.push_back(
					std::make_pair(m_vectorValuedResults[VVR_HeatingSetpoint].dataPtr() + i, m_valueRefs[1+i]) );
		resultInputValueReferences.push_back(
					std::make_pair(m_vectorValuedResults[VVR_CoolingSetpoint].dataPtr() + i, m_valueRefs[1+i]) );
	}
}


int ThermostatModel::update() {

	return 0; // signal success
}


} // namespace NANDRAD_MODEL
