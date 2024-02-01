/*	SIM-VICUS - Building and District Energy Simulation Tool.

	Copyright (c) 2020-today, Institut für Bauklimatik, TU Dresden, Germany

	Primary authors:
	  Andreas Nicolai  <andreas.nicolai -[at]- tu-dresden.de>
	  Dirk Weiss  <dirk.weiss -[at]- tu-dresden.de>
	  Stephan Hirth  <stephan.hirth -[at]- tu-dresden.de>
	  Hauke Hirsch  <hauke.hirsch -[at]- tu-dresden.de>

	  ... all the others from the SIM-VICUS team ... :-)

	This program is part of SIM-VICUS (https://github.com/ghorwin/SIM-VICUS)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
*/

#ifndef SVUndoTrimObjectsH
#define SVUndoTrimObjectsH

#include <VICUS_Surface.h>
#include <VICUS_Project.h>

#include "SVUndoCommandBase.h"

/*! Action for modifying a surface.
	This action is also used for adding/removing sub-surfaces.

	\warning IDs of modified surfaces MUST NOT change!
*/
class SVUndoTrimObjects : public SVUndoCommandBase {
	Q_DECLARE_TR_FUNCTIONS(SVUndoTrimObjects)
public:
	/*! Creates the undo-action.
		Mind: surfaces must be triangulated already, if they contain new sub-surfaces.
	*/
	SVUndoTrimObjects(const QString & label,
					  std::map<unsigned int, std::vector<IBKMK::Polygon3D>>	trimmedPolygons,
					  std::map<unsigned int, std::vector<IBKMK::Polygon3D>>	trimmedSubsurfaces,
					  const VICUS::Project & newProject);

	virtual void undo();
	virtual void redo();

private:

	/*! Object copies of surfaces to be deleted and added.
		key is id of surface to be trimmed
		value is vector with newly produced surfaces
	*/
	std::map<unsigned int, std::vector<IBKMK::Polygon3D>>	m_trimmedPolygons;
	std::map<unsigned int, std::vector<IBKMK::Polygon3D>>	m_trimmedSubsurfaces;

	/*! Copies of surface component instances. */
	//std::vector<VICUS::ComponentInstance>														m_compInstances;

	/*! Cache for entire project data. */
	VICUS::Project	m_project;

};


#endif // SVUndoTrimObjectsH