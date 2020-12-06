#include "Vic3DNewPolygonObject.h"

#include <QVector3D>
#include <QOpenGLShaderProgram>
#include <QElapsedTimer>

#include <VICUS_Project.h>
#include <VICUS_Conversions.h>

#include "SVProjectHandler.h"
#include "Vic3DGeometryHelpers.h"
#include "Vic3DCoordinateSystemObject.h"
#include "Vic3DShaderProgram.h"

namespace Vic3D {

NewPolygonObject::NewPolygonObject() :
	m_vertexBufferObject(QOpenGLBuffer::VertexBuffer), // VertexBuffer is the default, so default constructor would have been enough
	m_indexBufferObject(QOpenGLBuffer::IndexBuffer) // make this an Index Buffer
{
}


void NewPolygonObject::create(ShaderProgram * shaderProgram, const CoordinateSystemObject * coordSystemObject) {
	m_shaderProgram = shaderProgram;
	m_coordSystemObject = coordSystemObject;

	// *** create buffers on GPU memory ***

	// create a new buffer for the vertices and colors, separate buffers because we will modify colors way more often than geometry
	m_vertexBufferObject.create();
	m_vertexBufferObject.setUsagePattern(QOpenGLBuffer::StaticDraw); // usage pattern will be used when tranferring data to GPU

	// create a new buffer for the indexes
	m_indexBufferObject = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer); // Note: make sure this is an index buffer
	m_indexBufferObject.create();
	m_indexBufferObject.setUsagePattern(QOpenGLBuffer::StaticDraw);


	// *** create and bind Vertex Array Object ***

	// Note: VAO must be bound *before* the element buffer is bound,
	//       because the VAO remembers associated element buffers.
	m_vao.create();
	m_vao.bind(); // now the VAO is active and remembers states modified in following calls

	m_indexBufferObject.bind(); // this registers this index buffer in the currently bound VAO


	// *** set attribute arrays for shader fetch stage ***

#define VERTEX_ARRAY_INDEX 0

	m_vertexBufferObject.bind(); // this registers this buffer data object in the currently bound vao; in subsequent
				  // calls to shaderProgramm->setAttributeBuffer() the buffer object is associated with the
				  // respective attribute array that's fed into the shader. When the vao is later bound before
				  // rendering, this association is remembered so that the vertex fetch stage pulls data from
				  // this vbo

	// coordinates
	m_shaderProgram->shaderProgram()->enableAttributeArray(VERTEX_ARRAY_INDEX);
	m_shaderProgram->shaderProgram()->setAttributeBuffer(VERTEX_ARRAY_INDEX, GL_FLOAT, 0, 3 /* vec3 */, sizeof(VertexC));

	// Release (unbind) all

	// Mind: you can release the buffer data objects (vbo) before or after releasing vao. It does not
	//       matter, because the buffers are associated already with the attribute arrays.
	//       However, YOU MUST NOT release the index buffer (ebo) before releasing the vao, since this would remove
	//       the index buffer association with the vao and when binding the vao before rendering, the element buffer
	//       would not be known and a call to glDrawElements() crashes!
	m_vao.release();

	m_vertexBufferObject.release();
	m_indexBufferObject.release();

	m_planeGeometry = VICUS::PlaneGeometry(VICUS::PlaneGeometry::T_Polygon);
	// add test data
#if 0
	appendVertex(IBKMK::Vector3D(-5,0,0));
	appendVertex(IBKMK::Vector3D(0,2,0));
	appendVertex(IBKMK::Vector3D(-5,2,0));
#endif
}


void NewPolygonObject::destroy() {
	m_vao.destroy();
	m_vertexBufferObject.destroy();
	m_indexBufferObject.destroy();
}


void NewPolygonObject::appendVertex(const IBKMK::Vector3D & p) {
	m_planeGeometry.addVertex(p);
	updateBuffers();
}


void NewPolygonObject::updateLastVertex(const QVector3D & p) {
	// no vertex added yet? should normally not happen, but during testing we just check it
	if (m_vertexBufferData.empty())
		return;
	// any change to the previously stored point?
	if (p == m_vertexBufferData.back().m_coords)
		return;
	// update last coordinate
	m_vertexBufferData.back().m_coords = p;
	// and update the last part of the buffer (later, for now we just upload the entire buffer again)
	// transfer data stored in m_vertexBufferData
	m_vertexBufferObject.bind();
	m_vertexBufferObject.allocate(m_vertexBufferData.data(), m_vertexBufferData.size()*sizeof(VertexC));
	m_vertexBufferObject.release();
}


void NewPolygonObject::updateBuffers() {
	// create geometry

	// memory layout:
	//   with valid polygon:          vertexBuffer = |polygon_vertexes|coordinate system vertex|
	//   without valid polygon:       vertexBuffer = |last_polygon_vertex|coordinate system vertex|

	// index buffer is only filled if valid polygon exists

	// first copy polygon from PlaneGeometry, if at least 3 vertexes are inserted
	// then add vertex to last

	m_vertexBufferData.clear();
	m_indexBufferData.clear();
	unsigned int currentVertexIndex = 0;
	unsigned int currentElementIndex = 0;
	m_firstLineVertex = 0;

	// no vertexes, nothing to draw - we need at least one vertex in the geometry, so that we
	// can draw a line from the last vertex to the current coordinate system's location
	if (m_planeGeometry.vertexes().empty())
		return;

	if (m_planeGeometry.isValid()) {
		addPlane(m_planeGeometry, currentVertexIndex, currentElementIndex,
				 m_vertexBufferData, m_indexBufferData);
		// remember index of vertex where "current" line starts
		m_firstLineVertex = currentVertexIndex-1;
	}
	else {
		m_vertexBufferData.resize(1);
		m_vertexBufferData.back().m_coords = VICUS::IBKVector2QVector(m_planeGeometry.vertexes().back());
	}

	// now also add a vertex for the current coordinate system's position
	m_vertexBufferData.resize(m_vertexBufferData.size()+1);
	m_vertexBufferData.back().m_coords = m_coordSystemObject->m_transform.translation();

	// transfer data stored in m_vertexBufferData
	m_vertexBufferObject.bind();
	m_vertexBufferObject.allocate(m_vertexBufferData.data(), m_vertexBufferData.size()*sizeof(VertexC));
	m_vertexBufferObject.release();

	if (!m_indexBufferData.empty()) {
		m_indexBufferObject.bind();
		m_indexBufferObject.allocate(m_indexBufferData.data(), m_indexBufferData.size()*sizeof(GLshort));
		m_indexBufferObject.release();
	}

}


void NewPolygonObject::render() {
	// bind all buffers
	m_vao.bind();
	// set transformation matrix - unity matrix, since we draw with world coordinates
	m_shaderProgram->shaderProgram()->setUniformValue(m_shaderProgram->m_uniformIDs[1], QMatrix4x4());

	// now draw the geometry - first the polygon (if any)
	if (!m_indexBufferData.empty()) {
		glDisable(GL_CULL_FACE);
		// set wireframe color (TODO : make this theme-dependent?)
		QColor wireFrameCol = QColor(255,255,255,192);
		m_shaderProgram->shaderProgram()->setUniformValue(m_shaderProgram->m_uniformIDs[2], wireFrameCol);
		// put OpenGL in offset mode
		glEnable(GL_POLYGON_OFFSET_LINE);
		// offset the wire frame geometry a bit
		glPolygonOffset(0.0f, -2.0f);
		// select wire frame drawing
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		// now draw the geometry
		glDrawElements(GL_TRIANGLES, m_indexBufferData.size(), GL_UNSIGNED_SHORT, nullptr);
		// switch back to fill mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		// turn off line offset mode
		glDisable(GL_POLYGON_OFFSET_LINE);

		// set selected plane color (QColor is passed as vec4, so no conversion is needed, here).
		QColor planeCol = QColor(255,0,128,64);
		m_shaderProgram->shaderProgram()->setUniformValue(m_shaderProgram->m_uniformIDs[2], planeCol);
		// now draw the geometry
		glDrawElements(GL_TRIANGLES, m_indexBufferData.size(), GL_UNSIGNED_SHORT, nullptr);

		glEnable(GL_CULL_FACE);

	}
	if (m_vertexBufferData.size() > 1) {
		QColor lineCol = QColor(255,0,0,192);
		m_shaderProgram->shaderProgram()->setUniformValue(m_shaderProgram->m_uniformIDs[2], lineCol);
		// then the line consisting of the last two vertexes
		glLineWidth(2);
		glDrawArrays(GL_LINES, m_firstLineVertex, 2);
		glLineWidth(1);
	}
	// release buffers again
	m_vao.release();
}

} // namespace Vic3D
