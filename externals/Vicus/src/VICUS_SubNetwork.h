#ifndef SUBNETWORK_H
#define SUBNETWORK_H

#include "VICUS_AbstractDBElement.h"

#include "VICUS_Constants.h"
#include "VICUS_CodeGenMacros.h"
#include "VICUS_Database.h"
#include "VICUS_NetworkComponent.h"
#include "VICUS_NetworkController.h"
#include "VICUS_NetworkElement.h"
#include "VICUS_BMNetwork.h"
#include "VICUS_NetworkHeatExchange.h"
#include "tinyxml.h"

#include <NANDRAD_HydraulicNetworkElement.h>

#include <QColor>

namespace VICUS {

/*! Defines the structure of a sub-network (e.g. building heat exchanger with control value) that
	can be instantiated several times in different nodes.
	SubNetwork objects are referenced by Node objects.

	A sub-network is a template for a structure/network of individual flow elements that is used many times
	in the global network. Individual instances of a sub-network only differ in the heat exchange parametrization,
	e.g. energy loss/gain in buildings.

	There can be only one element in a sub-network with heat exchange.
*/
class SubNetwork : public AbstractDBElement {
public:

	// *** PUBLIC MEMBER FUNCTIONS ***

	VICUS_READWRITE_PRIVATE
	VICUS_COMP(SubNetwork)
	VICUS_COMPARE_WITH_ID

	/*! Checks if all referenced materials exist and if their parameters are valid. */
	bool isValid(const Database<Schedule> &scheduleDB) const;

	/*! updates the BMBlocks in the graphical Network with data from the NetworkElements */
	void init();

	/*! reads the subnetwork from XML, calls init() */
	void readXML(const TiXmlElement *element) override;

	/*! writes the subnetwork to XML */
	TiXmlElement* writeXML(TiXmlElement *parent) const override;

	/*! Comparison operator */
	ComparisonResult equal(const AbstractDBElement *other) const override;

	enum heatExchangeReturnStatus {
		RS_None,
		RS_TooMany,
		RS_Unique,
		NUM_RS
	};

	struct heatExchangeComponentStruct {
		heatExchangeReturnStatus status = RS_None;
		NetworkComponent component;
	};

	/*! Access function to the component of the heat exchanging element.
		Returns a struct that informs if there is no heatexchanger, exactly one or many heatexchangers
		in the Subnetwork.
		if there is exactly one HeatExchanger in the Subnetwork, it will return the component of that Heat exchanger
	*/
	heatExchangeComponentStruct heatExchangeComponent() const;

	//:inherited	unsigned int					m_id = INVALID_ID;							// XML:A:required
	//:inherited	IBK::MultiLanguageString		m_displayName;								// XML:A
	//:inherited	QColor							m_color;									// XML:A

	/*! Defines sub-network through elements, connected by implicitely numbered internal nodes.
		Nodes with INLET_ID and OUTLET_ID represent inlet and outlet nodes of the sub network respectively
		There must be only one inlet and one outlet node!
		NOTE: the heat exchange property of the elements are not used, instead, we assign the heat exchange property
		of the correspoinding node (the parent of this SubNetwork) using the heatExchangeElementId
	*/
	std::vector<NetworkElement>							m_elements;								// XML:E

	/*! Stores id of element with heat exchange parameterization. INVALID_ID means no heat exchange. */
	unsigned int										m_idHeatExchangeElement = INVALID_ID;	// XML:A

	/* stores all components used in this subnetwork */
	std::vector<NetworkComponent>                       m_components;                           // XML:E

	/*! Holds graphical information of the elements, contains BMBlocks and BMConnectors */
	BMNetwork                                           m_graphicalNetwork;                     // XML:E:tag=GraphicalNetwork

};



} // Namespace VICUS


#endif // SUBNETWORK_H
