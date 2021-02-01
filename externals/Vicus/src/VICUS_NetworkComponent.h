#ifndef VICUS_NetworkComponentH
#define VICUS_NetworkComponentH

#include <IBK_MultiLanguageString.h>

#include <QString>
#include <QColor>

#include <NANDRAD_HydraulicNetworkComponent.h>

#include "VICUS_CodeGenMacros.h"
#include "VICUS_Constants.h"
#include "VICUS_AbstractDBElement.h"
#include "VICUS_Database.h"

namespace VICUS {

/*! Data model object for network components, extends NANDRAD::HydraulicNetworkComponent with
	data members needed in the user interface.

	Note: Serialization is done in custom read/write function, where we call the generated
		readXML() and writeXML() functions manually.
*/
class NetworkComponent : public AbstractDBElement, public NANDRAD::HydraulicNetworkComponent {
	VICUS_READWRITE_PRIVATE
public:

	// *** PUBLIC MEMBER FUNCTIONS ***

	VICUS_READWRITE
	VICUS_COMPARE_WITH_ID
	VICUS_COMP(NetworkComponent)

	/*! Checks if all parameters are valid. */
	bool isValid() const;

	// *** PUBLIC MEMBER VARIABLES ***


	/*! Display name. */
	IBK::MultiLanguageString		m_displayName;								// XML:A

	/*! False color. */
	QColor							m_color;									// XML:A

	/*! Notes. */
	IBK::MultiLanguageString		m_notes;									// XML:E

	/*! Manufacturer. */
	IBK::MultiLanguageString		m_manufacturer;								// XML:E

	/*! Data source. */
	IBK::MultiLanguageString		m_dataSource;								// XML:E
};

} // namespace VICUS


#endif // VICUS_NetworkComponentH
