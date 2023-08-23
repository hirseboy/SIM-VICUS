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

#ifndef SVView3DDialogH
#define SVView3DDialogH

#include <vector>

#include <VICUS_Surface.h>

#include <IBKMK_Vector3D.h>

#include <IBK_Path.h>

#include <QString>
#include <QDialog>

/*! A dialog to export View3D data, run the external View3D tool and read back the results. */
class SVView3DDialog : public QWidget {
	Q_OBJECT
public:

	/*! Struct to hold all data for a View3D Vertex */
	struct view3dVertex {
		view3dVertex() = default;

		view3dVertex(unsigned int id, const IBKMK::Vector3D &v) :
			m_id(id),
			m_vertex(v)
		{
		}

		view3dVertex(unsigned int id, double x, double y, double z) :
			m_id(id),
			m_vertex(IBKMK::Vector3D (x, y, z) )
		{
		}

		unsigned int		m_id;		///> id of vertex

		IBKMK::Vector3D		m_vertex;	///> point of vertex with coordinates
	};

	/*! Struct to hold all data for a View3D Surface */
	struct view3dSurface {
		view3dSurface() = default;

		view3dSurface(unsigned int id, unsigned int vicusId, unsigned int v1, unsigned int v2, unsigned int v3, unsigned int v4,
					  unsigned int combId, double emittance, const std::string &name) :
			m_id(id),
			m_vicusId(vicusId),
			m_v1(v1),
			m_v2(v2),
			m_v3(v3),
			m_v4(v4),
			m_combId(combId),
			m_emittance(emittance),
			m_name(name)
		{
		}

		unsigned int					m_id;			///> id of surface

		unsigned int					m_vicusId;		///> corresponding vicus id

		unsigned int					m_v1;			///> id of first vertex
		unsigned int					m_v2;			///> id of second vertex
		unsigned int					m_v3;			///> id of third vertex
		unsigned int					m_v4;			///> id of fourth vertex

		unsigned int					m_combId;		///> id of surface that combines

		double							m_emittance;	///> emittance of surface

		std::string						m_name;			///> surface name for export
	};


	/*! Struct to hold all data for combining view factors */
	struct view3dExtendedSurfaces {
		view3dExtendedSurfaces() = default;

		view3dExtendedSurfaces( const unsigned int idVicusSurf ) :
			m_idVicusSurface(idVicusSurf)
		{
		}

		view3dExtendedSurfaces( const unsigned int idVicusSurf, bool isSubSurface ) :
			m_idVicusSurface(idVicusSurf),
			m_isSubSurface(isSubSurface)
		{
		}

		const unsigned int							m_idVicusSurface = VICUS::INVALID_ID;		///> id of Vicus Surface

		std::map<const unsigned int, double>		m_vicSurfIdToViewFactor;					///> Map with pointer to Surface and viewfactor

//		std::map<const VICUS::SubSurface*, double>	m_vicSubSurfToViewFactor;					///> Map with pointer to Surface and viewfactor

		bool										m_isSubSurface = false;						///> indicates wheather its a subsurface
	};


	/*! Struct to hold all data for a View3D Room with all surfaces and vertexes */
	struct view3dRoom {

		view3dRoom() = default;

		view3dRoom(unsigned int roomId, QString displayName) :
			m_roomId(roomId),
			m_displayName(displayName)
		{
		}

		unsigned int							m_roomId;					///> Room Id

		QString									m_displayName;				///> Room display name

		std::vector<view3dSurface>				m_surfaces;					///> Vector with all surfaces
		std::vector<view3dVertex>				m_vertexes;					///> Vector with all vertexes

		std::vector<view3dExtendedSurfaces>		m_extendedSurfaces;			///> Extended surfaces with all data
	};


	/*! Exports a View3D File */
	void exportView3d(std::vector<const VICUS::Surface *> selSurfaces, QWidget *parent);

	/*! Reads an View3D Log file with results */
	void readView3dResults(IBK::Path fname, view3dRoom &v3dRoom);


	std::map<unsigned int, view3dRoom>						m_vicusRoomIdToView3dRoom;		///> Map with View3D Rooms


	std::vector<VICUS::Surface>								m_modifiedSurfaces;				///>modified surfaces for the Undo Action

};

#endif // SVView3DDialogH
