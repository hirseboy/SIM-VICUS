/*	The NANDRAD data model library.

	Copyright (c) 2012-today, Institut für Bauklimatik, TU Dresden, Germany

	Primary authors:
	  Andreas Nicolai  <andreas.nicolai -[at]- tu-dresden.de>
	  Anne Paepcke     <anne.paepcke -[at]- tu-dresden.de>

	This library is part of SIM-VICUS (https://github.com/ghorwin/SIM-VICUS)

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
*/

#include "NANDRAD_EmbeddedObject.h"

#include <IBK_Exception.h>
#include <IBK_StringUtils.h>
#include <IBK_messages.h>

#include <tinyxml.h>

#include "NANDRAD_KeywordList.h"
#include "NANDRAD_Project.h"

namespace NANDRAD {

EmbeddedObject::objectType_t EmbeddedObject::objectType() const {

	if (m_window.hasParameters())
		return OT_WINDOW;

	return EmbeddedObject::NUM_OT;
}


void EmbeddedObject::checkParameters(const Project & prj) {
	double area = m_para[P_Area].checkedValue("m2", "m2", 0, true, std::numeric_limits<double>::max(), true,
											  "Cross section area of embedded object must be >= 0 m2.");

	m_window.checkParameters(area, prj.m_materials, prj.m_glazingSystems);
}


} // namespace NANDRAD

