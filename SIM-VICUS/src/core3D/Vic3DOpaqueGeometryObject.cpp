#include "Vic3DOpaqueGeometryObject.h"

#include <QVector3D>
#include <QOpenGLShaderProgram>
#include <QElapsedTimer>

namespace Vic3D {

OpaqueGeometryObject::OpaqueGeometryObject() :
	m_vertexBufferObject(QOpenGLBuffer::VertexBuffer), // VertexBuffer is the default, so default constructor would have been enough
	m_colorBufferObject(QOpenGLBuffer::VertexBuffer),
	m_indexBufferObject(QOpenGLBuffer::IndexBuffer) // make this an Index Buffer
{
}


void OpaqueGeometryObject::create(QOpenGLShaderProgram * shaderProgramm) {
	if (m_vao.isCreated())
		return;

	// *** create buffers on GPU memory ***

	// create a new buffer for the vertices and colors, separate buffers because we will modify colors way more often than geometry
	m_vertexBufferObject.create();
	m_vertexBufferObject.setUsagePattern(QOpenGLBuffer::StaticDraw); // usage pattern will be used when tranferring data to GPU

	m_colorBufferObject.create();
	m_colorBufferObject.setUsagePattern(QOpenGLBuffer::StaticDraw);

	// create a new buffer for the indexes
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
#define NORMAL_ARRAY_INDEX 1
#define COLOR_ARRAY_INDEX 2

	m_vertexBufferObject.bind(); // this registers this buffer data object in the currently bound vao; in subsequent
				  // calls to shaderProgramm->setAttributeBuffer() the buffer object is associated with the
				  // respective attribute array that's fed into the shader. When the vao is later bound before
				  // rendering, this association is remembered so that the vertex fetch stage pulls data from
				  // this vbo

	// coordinates
	shaderProgramm->enableAttributeArray(VERTEX_ARRAY_INDEX);
	shaderProgramm->setAttributeBuffer(VERTEX_ARRAY_INDEX, GL_FLOAT, 0, 3 /* vec3 */, sizeof(Vertex));

	// normals
	shaderProgramm->enableAttributeArray(NORMAL_ARRAY_INDEX);
	shaderProgramm->setAttributeBuffer(NORMAL_ARRAY_INDEX, GL_FLOAT, offsetof(Vertex, nx), 3 /* vec3 */, sizeof(Vertex));


	m_colorBufferObject.bind(); // now color buffer is active in vao

	// colors
	shaderProgramm->enableAttributeArray(COLOR_ARRAY_INDEX);
	shaderProgramm->setAttributeBuffer(COLOR_ARRAY_INDEX, GL_UNSIGNED_BYTE, 0, 4, 4 /* bytes = sizeof(char) */);

	// Release (unbind) all

	// Mind: you can release the buffer data objects (vbo and vboColors) before or after releasing vao. It does not
	//       matter, because the buffers are associated already with the attribute arrays.
	//       However, YOU MUST NOT release the index buffer (ebo) before releasing the vao, since this would remove
	//       the index buffer association with the vao and when binding the vao before rendering, the element buffer
	//       would not be known and a call to glDrawElements() crashes!
	m_vao.release();

	m_vertexBufferObject.release();
	m_colorBufferObject.release();
	m_indexBufferObject.release();
}


void OpaqueGeometryObject::destroy() {
	m_vao.destroy();
	m_vertexBufferObject.destroy();
	m_colorBufferObject.destroy();
	m_indexBufferObject.destroy();
}


void OpaqueGeometryObject::updateBuffers() {

#define SET_TESTDATA
#ifdef SET_TESTDATA
	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------

	float vertices[] = {
		 0.8f,  0.8f, 0.0f,  // top right		= 0
		 0.8f, -0.8f, 0.0f,  // bottom right	= 1
		-0.8f,  0.8f, 0.0f,   // top left		= 2
		-0.8f, -0.8f, 0.0f,  // bottom left		= 3
		1.8f,  0.8f, 0.0f,  // top right		= 0
		1.8f, -0.8f, 0.0f,  // bottom right	= 1
		1.0f,  0.8f, 0.0f,   // top left		= 2
		1.0f, -0.8f, 0.0f  // bottom left		= 3
	};

	QColor vertexColors [] = {
		// left rect

		QColor("#ff0000"),  // red    // top right
		QColor("#00ff00"), // green  // bottom right
		QColor("#0000ff"), // blue   // top left
		QColor("#ffffff"), // white  // bottom left

		// right rect

		QColor("#ff0000"),	// red - top right
		QColor("#ff00ff"),	// magenta - bottom right
		QColor("#ffffff"),	// white - top left
		QColor("#00ff00")	// green - bottom left
	};

	// create buffer for 2 interleaved attributes: position and color, 4 vertices, 3 floats each
	unsigned int N_Vertices = 8;
	m_vertexBufferData.resize(N_Vertices);
	m_colorBufferData.resize(N_Vertices);
	for (int v=0; v<N_Vertices; ++v) {
		// coordinates
		m_vertexBufferData[v].x = 100*vertices[3*v]/2;
		m_vertexBufferData[v].y = 100*vertices[3*v+1]/2;
		m_vertexBufferData[v].z = 100*vertices[3*v+2]/2;

		// colors
		m_colorBufferData[v] = vertexColors[v];
	}

	//#define USE_DEGENERATED_TRIANGLE_RESTART
	#ifdef USE_DEGENERATED_TRIANGLE_RESTART
		GLushort indices[] = {  // note that we start from 0!
			0, 1, 2, 3,
			3, // duplicate last of first strip
			4, // duplicate first of second strip
			4, 5, 6, 7
		};

		// Note: when inserting a gap from even to odd element, insert 2; otherweise 3
		m_indexCount = 10;
	#else

#ifdef Q_OS_MAC
		glEnable(GL_PRIMITIVE_RESTART);
		glPrimitiveRestartIndex(0xFFFF);
#else
		glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#endif // Q_OS_MAC

		GLushort indices[] = {  // note that we start from 0!
			0, 1, 2, 3,
			0xFFFF,
			4, 5, 6, 7
		};
#endif // USE_DEGENERATED_TRIANGLE_RESTART

	m_elementBufferData = std::vector<GLshort>(indices, indices + 9);
#endif // SET_TESTDATA

	if (m_elementBufferData.empty())
		return;

	// transfer data stored in m_vertexBufferData
	m_vertexBufferObject.bind();
	m_vertexBufferObject.allocate(m_vertexBufferData.data(), m_vertexBufferData.size()*sizeof(Vertex));
	m_vertexBufferObject.release();

	m_indexBufferObject.bind();
	m_indexBufferObject.allocate(m_elementBufferData.data(), m_elementBufferData.size()*sizeof(GL_UNSIGNED_SHORT));
	m_indexBufferObject.release();

	// also update the color buffer
	updateColorBuffer();
}


void OpaqueGeometryObject::updateColorBuffer() {
	if (m_colorBufferData.empty())
		return;
	m_colorBufferObject.bind();
	m_colorBufferObject.allocate(m_colorBufferData.data(), m_colorBufferData.size()*sizeof(ColorRGBA) );
	m_colorBufferObject.release();
}


void OpaqueGeometryObject::render() {
	// bind all buffers ("position", "normal" and "color" arrays)
	m_vao.bind();
	// now draw the geometry
	glDrawElements(GL_TRIANGLE_STRIP, m_elementBufferData.size(), GL_UNSIGNED_SHORT, nullptr);
	// release buffers again
	m_vao.release();
}

} // namespace Vic3D
