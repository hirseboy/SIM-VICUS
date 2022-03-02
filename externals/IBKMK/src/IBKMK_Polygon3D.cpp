/*	IBK Math Kernel Library
	Copyright (c) 2001-today, Institut fuer Bauklimatik, TU Dresden, Germany

	Written by A. Nicolai, A. Paepcke, H. Fechner, St. Vogelsang
	All rights reserved.

	This file is part of the IBKMK Library.

	Redistribution and use in source and binary forms, with or without modification,
	are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	   list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its contributors
	   may be used to endorse or promote products derived from this software without
	   specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
	ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
	ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	This library contains derivative work based on other open-source libraries,
	see LICENSE and OTHER_LICENSES files.

*/

#include "IBKMK_Polygon3D.h"

#include <IBK_Line.h>
#include <IBK_math.h>
#include <IBK_messages.h>

#include "IBKMK_3DCalculations.h"

namespace IBKMK {

Polygon3D::Polygon3D(Polygon2D::type_t t, const IBKMK::Vector3D & a, const IBKMK::Vector3D & b, const IBKMK::Vector3D & c) {
//	if (t == Polygon2D::T_Rectangle) {
//		// third vertex is actually point d of the rectangle, so we first set vertex[3] = vertex[2],
//		// then compute vertex [c] again
//		m_vertexes.push_back(m_vertexes.back());
//		// c = a + (b-a) + (d-a) = b + (d - a)
//		m_vertexes[2] = m_vertexes[1] + (m_vertexes[3]-m_vertexes[0]);
//	}
//	checkPolygon(); // this also safeguards against a == b or b == c or a == c inputs
}


Polygon3D::Polygon3D(const Polygon2D & polyline, const IBKMK::Vector3D & normal,
					 const IBKMK::Vector3D & localX, const IBKMK::Vector3D & offset) :
	m_offset(offset), m_polyline(polyline)
{
	// guard against invalid polylines
	if (polyline.isValid())
		setRotation(normal, localX);
}


Polygon3D::Polygon3D(const std::vector<IBKMK::Vector3D> & vertexes) {
	// we construct a polygon from points by first eliminating collinear points, then checking

	m_valid = false;
	m_polyline.clear();
	if (vertexes.size() < 3)
		return;

	// eliminate colliniear points in temporary vector
	std::vector<IBKMK::Vector3D> verts(vertexes);
	IBKMK::eliminateCollinearPoints(verts);

	updateLocalCoordinateSystem(verts);
	// we need 3 vertexes (not collinear) to continue and a valid normal vector!
	if (m_vertexes.size() < 3 || m_normal == IBKMK::Vector3D(0,0,0))
		return;

	update2DPolyline();

//	// polygon must not be winding into itself, otherwise triangulation would not be meaningful
//	m_valid = m_polyline.isValid() && m_polyline.isSimplePolygon();

//	if (m_valid && m_vertexes.size() != m_polyline.vertexes().size()) {
//		// When computing polyline we correct vertexes that are out of plane
//		// hereby, the corrected vertexes may be closer together than the original vertexes.
//		// When constructing the polyline, these vertexes will be removed.
//		// In these situations we re-compute 3D vertexes from polyline to have again
//		// a consistent polygon. Note: if the polyline is valid, we have at least 3 vertexes.
//		update3DVertexesFromPolyline(m_vertexes[0]);
//	}

}


// Comparison operator !=
bool Polygon3D::operator!=(const Polygon3D &other) const {

	// TODO

	return false;
}


const std::vector<Vector3D> & Polygon3D::vertexes() const {
	if (m_dirty) {
		// recompute 3D vertex cache

		// TODO

		m_dirty = false;
	}
	return m_vertexes;
}


// *** Transformation Functions ***

void Polygon3D::setRotation(const IBKMK::Vector3D & normal, const IBKMK::Vector3D & xAxis) {
	FUNCID(Polygon3D::setRotation);

	if (!IBK::nearly_equal<6>(normal.magnitudeSquared(), 1.0))
		throw IBK::Exception("Normal vector does not have unit length!", FUNC_ID);
	if (!IBK::nearly_equal<6>(xAxis.magnitudeSquared(), 1.0))
		throw IBK::Exception("xAxis vector does not have unit length!", FUNC_ID);
	// check that the vectors are (nearly) orthogonal
	double sp = normal.scalarProduct(xAxis);
	if (!IBK::nearly_equal<6>(sp, 1.0))
		throw IBK::Exception("Normal and xAxis vectors must be orthogonal!", FUNC_ID);

	// we only modify our vectors if all input data is correct - hence we ensure validity of the polygon
	m_normal = normal;
	m_localX = xAxis;
	normal.crossProduct(xAxis, m_localY); // Y = N x X - right-handed coordinate system
	m_localY.normalize();
	m_dirty = true; // mark 3D vertexes as dirty
}


void Polygon3D::flip() {
	IBK_ASSERT(isValid());
	m_normal = -1.0*m_normal; // Note: x and y axis remain the same!
	m_dirty = true;
}


// *** Calculation Functions ***


IBKMK::Vector3D Polygon3D::centerPoint() const {
	FUNCID(Polygon3D::centerPoint);
	if (!isValid())
		throw IBK::Exception("Invalid polygon.", FUNC_ID);

	size_t counter=0;
	IBKMK::Vector3D vCenter;

	for (const IBKMK::Vector3D & v : vertexes()) {
		vCenter += v;
		++counter;
	}
	vCenter/=static_cast<double>(counter);

	return vCenter;
}


void Polygon3D::boundingBox(Vector3D & lowerValues, Vector3D & upperValues) const {
	FUNCID("Polygon3D::boundingBox");
	if (!isValid())
		throw IBK::Exception("Invalid polygon.", FUNC_ID);
	// Note: do not access m_vertexes directly, as this array may be dirty
	const std::vector<Vector3D> & points = vertexes();
	// initialize bounding box with first point
	lowerValues = points[0];
	upperValues = points[0];
	for (unsigned int i=1; i<points.size(); ++i)
		IBKMK::enlargeBoundingBox(points[i], lowerValues, upperValues);
}


void Polygon3D::enlargeBoundingBox(Vector3D & lowerValues, Vector3D & upperValues) const {
	FUNCID("Polygon3D::enlargeBoundingBox");
	if (!isValid())
		throw IBK::Exception("Invalid polygon.", FUNC_ID);
	// Note: do not access m_vertexes directly, as this array may be dirty
	const std::vector<Vector3D> & points = vertexes();
	for (const IBKMK::Vector3D & v: points)
		IBKMK::enlargeBoundingBox(v, lowerValues, upperValues);
}



// *** PRIVATE MEMBER FUNCTIONS ***



void Polygon3D::updateLocalCoordinateSystem(const std::vector<IBKMK::Vector3D> & verts) {
	// NOTE: DO NOT ACCESS m_vertexes IN THIS FUNCTION!

	m_normal = IBKMK::Vector3D(0,0,0);
	if (verts.size() < 3)
		return;

	// We define our normal via the winding order of the polygon.
	// Since our polygon may be concave (i.e. have dents), we cannot
	// just pick any point and compute the normal via the adjacent edge vectors.
	// Instead, we first calculate the normal vector based on the first two edges.
	// Then, we loop around the entire polygon, compute the normal vectors at
	// each vertex and compare it with the first. If pointing in the same direction,
	// we count up, otherwise down. The direction with the most normal vectors wins
	// and will become our polygon's normal vector.

	// calculate normal with first 3 points
	m_localX = verts[1] - verts[0];
	IBKMK::Vector3D y = verts.back() - verts[0];
	IBKMK::Vector3D n;
	m_localX.crossProduct(y, n);
	// if we interpret n as area between y and localX vectors, this should
	// be reasonably large (> 1 mm2). If we, however, have a very small magnitude
	// the vectors y and localX are (nearly) collinear, which should have been prevented by
	// eliminateColliniarPoints() before.
	if (n.magnitude() < 1e-9)
		return; // invalid vertex input
	n.normalize();

	int sameDirectionCount = 0;

	// now process all other points and generate their normal vectors as well
	for (unsigned int i=1; i<verts.size(); ++i) {
		IBKMK::Vector3D vx = verts[(i+1) % verts.size()] - verts[i];
		IBKMK::Vector3D vy = verts[i-1] - verts[i];
		IBKMK::Vector3D vn;
		vx.crossProduct(vy, vn);
		// again, we check for not collinear points here (see explanation above)
		if (vn.magnitude() < 1e-9)
			return; // invalid vertex input
		vn.normalize();
		// adding reference normal to current vertexes normal and checking magnitude works
		if ((vn + n).magnitude() > 1) // can be 0 or 2, so comparing against 1 is good even for rounding errors
			++sameDirectionCount;
		else
			--sameDirectionCount;
	}

	if (sameDirectionCount < 0) {
		// invert our normal vector
		n *= -1;
	}

	// save-guard against degenerate polygons (i.e. all points close to each other or whatever error may cause
	// the normal vector to have near zero magnitude... this may happen for extremely small polygons, when
	// the x and y vector lengths are less than 1 mm in length).
	m_normal = n;
	// now compute local Y axis
	n.crossProduct(m_localX, m_localY);
	// normalize localX and localY
	m_localX.normalize();
	m_localY.normalize();
}



void Polygon3D::update2DPolyline(const std::vector<Vector3D> & verts) {
	// NOTE: DO NOT ACCESS m_vertexes IN THIS FUNCTION!

	IBK_ASSERT(verts.size() >= 3);

	std::vector<IBKMK::Vector2D> poly;
	poly.reserve(verts.size());

	// first point is v0 = origin
	poly.push_back( IBKMK::Vector2D(0,0) );

	// now process all other points
	for (unsigned int i=1; i<verts.size(); ++i) {
		const IBKMK::Vector3D & v = verts[i];
		double x,y;
		/// TODO: Dirk, improve this - by simply calling planeCoordinates we
		///       redo the same stuff several times for the same plane.
		///       We should use a function that passes vX, vY, offset and then
		///       a vector with v,x,y to process.
		if (IBKMK::planeCoordinates(verts[0], m_localX, m_localY, v, x, y)) {
			poly.push_back( IBKMK::Vector2D(x,y) );
		}
		else {
			return;
		}
	}
	m_polyline.setVertexes(poly); // Mind: this may lead to removal of points if two are close together
}


void IBKMK::Polygon3D::update3DVertexesFromPolyline(IBKMK::Vector3D offset) {
	// TODO : check this logic!
	const std::vector<IBKMK::Vector2D> &polylineVertexes = m_polyline.vertexes();
	m_vertexes.resize(polylineVertexes.size());
	// Mind: we may have the case, that due to collinear points in polyline we have removed vertex (0,0), and
	//       now the first 3D vertex no longer matches offset. Hence, we also compute the offset vertex again.
	for (unsigned int i=0; i<m_vertexes.size(); ++i)
		m_vertexes[i] = offset + m_localX * polylineVertexes[i].m_x + m_localY * polylineVertexes[i].m_y;
}

} // namespace IBKMK

