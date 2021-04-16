#include "NANDRAD_ShadingControlModel.h"

#include "NANDRAD_ConstructionInstance.h"
#include "NANDRAD_EmbeddedObject.h"
#include "NANDRAD_Sensor.h"

#include <NANDRAD_KeywordList.h>

#include <algorithm>

namespace NANDRAD {

void ShadingControlModel::checkParameters(const std::vector<Sensor> &sensors,
										  const std::vector<ConstructionInstance> &conInstances)
{
	FUNCID(ShadingControlModel::checkParameters);

	// parameter unit and range
	m_para[P_MaxIntensity].checkedValue("MaxIntensity", "W/m2", "W/m2",
							   0, true,
							   std::numeric_limits<double>::max(), true,
							   "MaxIntensity must be > 0.");

	m_para[P_MinIntensity].checkedValue("MinIntensity", "W/m2", "W/m2",
							   0, true,
							   std::numeric_limits<double>::max(), true,
							   "MinIntensity must be > 0.");

	if(m_para[P_MaxIntensity].value <= m_para[P_MinIntensity].value) {
		throw IBK::Exception(IBK::FormatString("MaxIntensity must be greater than MinIntensity."), FUNC_ID);
	}

	// retrieve network component
	std::vector<Sensor>::const_iterator sit =
			std::find(sensors.begin(), sensors.end(), m_sensorID);
	if (sit != sensors.end()) {
		// set reference
		m_sensor = &(*sit);
	}
	// search for construction instance
	else {
		// find construction instance id
		std::vector<NANDRAD::ConstructionInstance>::const_iterator conit =
				std::find(conInstances.begin(),
						  conInstances.end(),
						  m_sensorID);

		// store construction instance (contains orientation and inclination)
		if(conit != conInstances.end()) {

			// only an outside construction is accepted
			if(conit->interfaceAZoneID() != 0 && conit->interfaceBZoneID() != 0) {
				throw IBK::Exception(IBK::FormatString("Embedded object with id #%1 is part of an inside construction "
													   "and may therefore not be referenced.")
									 .arg(m_sensorID), FUNC_ID);
			}

			m_constructionInstance = &(*conit);
		}
		else {
			// search for embedded object
			for(const NANDRAD::ConstructionInstance &conInstance : conInstances) {
				// find embedded object id
				std::vector<NANDRAD::EmbeddedObject>::const_iterator embit =
						std::find(conInstance.m_embeddedObjects.begin(),
								  conInstance.m_embeddedObjects.end(),
								  m_sensorID);

				// store construction instance (contains orientation and inclination)
				if(embit != conInstance.m_embeddedObjects.end()) {
					// store pointer to embedded object?
					m_embeddedObject = &(*embit);
					break;
				}
			}
		}
	}

	if(m_sensor == nullptr && m_constructionInstance == nullptr && m_embeddedObject == nullptr)
		throw IBK::Exception(IBK::FormatString("Neither sensor nor construction instance nor embedded object with id #%1 does exist.")
							 .arg(m_sensorID), FUNC_ID);
}


} // namespace NANDRAD
