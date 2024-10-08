#ifndef VicOSM_BARRIER_H
#define VicOSM_BARRIER_H

#include "VicOSM_AbstractOSMObject.h"
#include "VicOSM_Way.h"

namespace VicOSM {

class Barrier : public AbstractOSMObject {
public:
	void readXML(const TiXmlElement * element);
	TiXmlElement * writeXML(TiXmlElement * parent) const;

	//:inherited	std::string						m_key = "";				// XML:A
	//:inherited	std::string						m_value = "";			// XML:A
	//:inherited	double							m_zPosition = 0;		// XML:A
	//:inherited	std::vector<Area>				m_areas;				// XML:E
	//:inherited	std::vector<LineFromPlanes>		m_linesFromPlanes;		// XML:E
	//:inherited	std::vector<Circle>				m_circles;				// XML:E

	static bool createBarrier(Way &way, Barrier &barrier);

};

} // namespace VicOSM

#endif // VicOSM_BARRIER_H
