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

#ifndef Vic3DRubberbandObjectH
#define Vic3DRubberbandObjectH

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include <QVector3D>
#include <QCoreApplication>


QT_BEGIN_NAMESPACE
class QOpenGLShaderProgram;
QT_END_NAMESPACE

#include "Vic3DTransform3D.h"
#include "Vic3DVertex.h"
#include "Vic3DCamera.h"

namespace ClipperLib {
class IntPoint;
}


namespace Vic3D {

class ShaderProgram;
class Scene;

extern float SCALE_FACTOR;

/*!

*/
class RubberbandObject {
	Q_DECLARE_TR_FUNCTIONS(Scene)
public:

	RubberbandObject();

	/*! The function is called during OpenGL initialization, where the OpenGL context is current. */
	void create(Scene *scene, ShaderProgram * opaquePhongShaderProgram);
	void destroy();

	/*! Resets the rubberband object. */
	void reset();

	/*! Binds the buffer and paints. */
	void render();

	/*! Set top-left point of rubberband. */
	void setStartPoint(const QVector3D &topLeft);

	/*! Set bottom right end point of rubberband. */
	void setRubberband(const QVector3D &bottomRight);

	/*! Converts a QVector3D to an ClipperLib Int-Point. */
	ClipperLib::IntPoint toClipperIntPoint(const QVector3D &p);

	/*! Updates the viewport rect of scene. */
	void setViewport(const QRect &viewport);

	/*! Select Objects based on rubberband.
		And also push the undo-action.

		1) Construct a polygon in scene based on rubberband.
		2) Take the sight vector from camera and construct viewing sight pane.
		3) Take all surface edge points.
		4) Project surface point onto camera sight pane.
		5) if projected point lies within polygon --> store point selection
		6) Based on touching mode	1) Full Mode: if all points of surface lie within polygon --> polygon selected
									2) Touching Mode: if at least 1 projected point lies within polygon --> polygon selected
		7) Construct undo-action with selection changed polygons
		8) push undo action.
	*/
	void selectObjectsBasedOnRubberband();

private:

	/*! Holds the number of vertices (2 for each line), updated in create(), used in render().
		If zero, grid is disabled.
	*/
	GLsizei						m_vertexCount;

	/*! Viewport. */
	QRect						m_viewport;

	QMatrix4x4					m_matrix;

	/*! QVector3D of top-left position of rubberband. */
	QVector3D					m_topLeft;

	/*! Coordinates of rubberband view specific. */
	QVector3D					m_topLeftView;
	QVector3D					m_bottomRightView;

	/*! Shader program. */
	ShaderProgram				*m_rubberbandShaderProgram = nullptr;

	/*! Wraps an OpenGL VertexArrayObject, that references the vertex coordinates. */
	QOpenGLVertexArrayObject	m_vao;
	/*! Handle for vertex buffer on GPU memory. */
	QOpenGLBuffer				m_vbo;

	/*! Select Geometry. */
	bool						m_selectGeometry = true;

	/*! Select touched objects. */
	bool						m_touchGeometry = true;

	/*! Vertex buffer in CPU memory, holds data of all vertices (coords and normals). */
	std::vector<Vertex>			m_vertexBufferData;
	/*! Color buffer in CPU memory, holds colors of all vertices (same size as m_vertexBufferData). */
	std::vector<ColorRGBA>		m_colorBufferData;
	/*! Index buffer on CPU memory. */
	std::vector<GLuint>			m_indexBufferData;

	unsigned int				m_planeStartIndex;
	unsigned int				m_planeStartVertex;

	/*! Pointer to scene. Needed for accessing camera. */
	Scene						*m_scene = nullptr;
};

} // namespace Vic3D


#endif // Vic3DRubberbandObjectH
