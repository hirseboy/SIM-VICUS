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

#include "IBKMK_2DCalculations.h"
#include "IBKMK_3DCalculations.h"

namespace IBKMK {

Polygon3D::Polygon3D(Polygon2D::type_t t, const IBKMK::Vector3D & a, const IBKMK::Vector3D & b, const IBKMK::Vector3D & c) {
	std::vector<IBKMK::Vector3D> verts;
	verts.push_back(a);
	verts.push_back(b);
	verts.push_back(c);
	if (t == Polygon2D::T_Rectangle) {
		// third vertex is actually point d of the rectangle, so we first set vertex[3] = vertex[2],
		// then compute vertex [c] again
		verts.push_back(verts.back());
		// c = a + (b-a) + (d-a) = b + (d - a)
		verts[2] = verts[1] + (verts[3]-verts[0]);
	}
	// now generate a Polygon3D
	(*this) = Polygon3D(verts);
}


Polygon3D::Polygon3D(const Polygon2D & polyline, const IBKMK::Vector3D & offset,
					 const IBKMK::Vector3D & normal, const IBKMK::Vector3D & localX) :
	m_offset(offset), m_polyline(polyline)
{
	FUNCID(Polygon3D::Polygon3D);
	// first point must be 0,0
	if (!polyline.isValid()) {
		m_valid = false;
		return;
	}
	if (polyline.vertexes()[0] != IBKMK::Vector2D(0,0)) {
		IBK_Message("First point of polyline must be 0,0!", IBK::MSG_ERROR, FUNC_ID);
		m_valid = false;
		return;
	}
	try {
		m_valid = true; // assume polygon is valid
		setRotation(normal, localX); // also sets the m_dirty flag
	} catch (...) {
		m_valid = false;
	}
}


Polygon3D::Polygon3D(const std::vector<IBKMK::Vector3D> & vertexes) {
	setVertexes(vertexes, false); // no automatic healing here! We want to know if vertexes are bad.
	// mark 3D vertexes as dirty
	m_dirty = true;
}


bool Polygon3D::setVertexes(const std::vector<Vector3D> & vertexes, bool heal) {
	FUNCID(Polygon3D::setVertexes);

	// we construct a polygon from points by first eliminating collinear points, then checking

	m_valid = false;
	m_polyline.clear();
	if (vertexes.size() < 3)
		return false;

	// eliminate colliniear points in temporary vector
	std::vector<IBKMK::Vector3D> verts(vertexes);
	IBKMK::eliminateCollinearPoints(verts);

	updateLocalCoordinateSystem(verts);
	// we need 3 vertexes (not collinear) to continue and a valid normal vector!
	if (verts.size() < 3 || m_normal == IBKMK::Vector3D(0,0,0))
		return false;

	// we now have a valid local coordinate sytstem, set our original to the first vertex of the remaining
	// vertexes, so that the polygon has always 0,0 as first point
	update2DPolyline(verts);

	// polygon must not be winding into itself, otherwise triangulation would not be meaningful
	m_valid = m_polyline.isValid() && m_polyline.isSimplePolygon();

	// if our polyline is not valid and we are requested to attempt healing, do this now
	if (heal && !m_polyline.isValid()) {
		IBK::IBK_Message("Attempting healing of polygon 3D.", IBK::MSG_DEBUG, FUNC_ID, IBK::VL_INFO);

		// we take a vector to hold our deviations, i.e. the sum of the vertical deviations from the plane.
		std::vector<double> deviations (verts.size(), 0);
		// create a vector to hold the projected points for each of the plane variants
		std::vector<std::vector<IBKMK::Vector3D> > projectedPoints ( verts.size(), std::vector<IBKMK::Vector3D> ( verts.size(), IBKMK::Vector3D (0,0,0) ) );

		// we iterate through all points and construct planes
		double smallestDeviation = std::numeric_limits<double>::max();
		unsigned int bestPlaneIndex = (unsigned int)-1;
		for (unsigned int i = 0, count = verts.size(); i<count; ++i ) {

			// select point i  as offset
			const IBKMK::Vector3D & offset = verts[i];

			// define plane through vectors a and b
			const IBKMK::Vector3D & a = verts[(i + 1)         % count] - offset;
			const IBKMK::Vector3D & b = verts[(i - 1 + count) % count] - offset;

			// we now iterate again through all points of the polygon and
			// sum up the distances of all points to their projection point
			for (unsigned int j = 0; j<count; ++j ) {

				// offset point?
				if ( i == j ) {
					projectedPoints[i][j] = offset;
					continue;
				}

				// we take the current point
				const IBKMK::Vector3D & vertex = verts[j];

				// we find our projected points onto the plane
				double x, y;
				// allow a fairly large tolerance here
				if (IBKMK::planeCoordinates(offset, a, b, vertex, x, y, 1e-2)) {

					// now we construct our projected points and find the deviation between the original points
					// and their projection
					projectedPoints[i][j] = offset + a*x + b*y;

					// add up the distance between original vertex and projected point
					// Note: if we add the square of the distances, we still get the maximum deviation, but avoid
					//       the expensive square-root calculation
					deviations[i] += (projectedPoints[i][j] - vertex).magnitudeSquared();
				}
				else {
					// if we cannot find a valid project point for any reason, store the original point and set
					// a very large distance
					projectedPoints[i][j] = vertex;
					deviations[i] += 100; // this will effectively eliminate this plane as option
				}
			}

			// remember plane index if new deviation is smallest
			if (deviations[i] < smallestDeviation) {
				bestPlaneIndex = i;
				smallestDeviation = deviations[i];
			}
		}

		// if one of the points is soo far out of the polygon, that fixing is not even possible, we note that as error
		if (smallestDeviation > 100) {
			std::stringstream strm;
			for (unsigned int i = 0, count = verts.size(); i<count; ++i )
				strm << verts[i].toString() << (i+1<count ? ", " : "");
			IBK::IBK_Message("Cannot fix polygon: " + strm.str(), IBK::MSG_DEBUG, FUNC_ID, IBK::VL_INFO);
			return false; // polygon remains invalid
		}

		// take the best vertex set and use it for the polygon
		setVertexes(projectedPoints[bestPlaneIndex], false); // Mind: we call our function again, but this time without healing to avoid looping
	}
	m_dirty = true;
	return m_valid;
}


// Comparison operator !=
bool Polygon3D::operator!=(const Polygon3D &other) const {

	if (m_valid != other.m_valid ||
			m_normal != other.m_normal ||
			m_offset != other.m_offset ||
			m_localX != other.m_localX ||
			m_polyline != other.m_polyline)
	{
		return true;
	}

	return false;
}


const std::vector<Vector3D> & Polygon3D::vertexes() const {
	FUNCID(Polygon3D::vertexes);
	if(m_polyline.vertexes().empty())
		throw IBK::Exception("Polyline does not contain any vertexes!", FUNC_ID);

	if (m_dirty) {
		// recompute 3D vertex cache

		const std::vector<IBKMK::Vector2D> &polylineVertexes = m_polyline.vertexes();
		m_vertexes.resize(polylineVertexes.size());
		m_vertexes[0] = m_offset;
		for (unsigned int i=1; i<m_vertexes.size(); ++i)
			m_vertexes[i] = m_offset + m_localX * polylineVertexes[i].m_x + m_localY * polylineVertexes[i].m_y;

		m_dirty = false;
	}
	return m_vertexes;
}

const std::vector<Vector3D> &Polygon3D::rawVertexes() const {
	return m_vertexes;
}


// *** Transformation Functions ***

void Polygon3D::setRotation(const IBKMK::Vector3D & normal, const IBKMK::Vector3D & xAxis) {
	FUNCID(Polygon3D::setRotation);

	if (!IBK::nearly_equal<4>(normal.magnitudeSquared(), 1.0))
		throw IBK::Exception("Normal vector does not have unit length!", FUNC_ID);
	if (!IBK::nearly_equal<4>(xAxis.magnitudeSquared(), 1.0))
		throw IBK::Exception("xAxis vector does not have unit length!", FUNC_ID);
	// check that the vectors are (nearly) orthogonal
	double sp = normal.scalarProduct(xAxis);
	if (!IBK::nearly_equal<4>(sp, 0.0))
		throw IBK::Exception("Normal and xAxis vectors must be orthogonal!", FUNC_ID);

	// we only modify our vectors if all input data is correct - hence we ensure validity of the polygon
	m_normal = normal;
	m_localX = xAxis;
	normal.crossProduct(xAxis, m_localY); // Y = N x X - right-handed coordinate system
	m_localY.normalize();
	m_dirty = true; // mark 3D vertexes as dirty
}


IBKMK::Vector2D Polygon3D::flip() {
	IBK_ASSERT(isValid());
	m_normal = -1.0*m_normal;
	// we need to swap x and y axes to keep right-handed coordinate system
	std::swap(m_localX, m_localY);

	// we also need to swap x and y coordinates of all polygon2D points
	std::vector<IBKMK::Vector2D>		vertexes2D = m_polyline.vertexes();
	std::vector<IBKMK::Vector2D>		vertexes2DNew;
	for (IBKMK::Vector2D & v : vertexes2D)
		std::swap(v.m_x, v.m_y);

	// attention update offset
	m_offset = m_offset + m_localX * vertexes2D.back().m_x + m_localY * vertexes2D.back().m_y;

	// so now we have to update all points in 2D to new offset point
	IBKMK::Vector2D offset2D = vertexes2D.back() - vertexes2D.front();

	// to comply with the flipped surface normal we also have to reverse the order 2D points
	for (unsigned int i=vertexes2D.size(); i>0; --i)
		vertexes2DNew.push_back(vertexes2D[i-1]-offset2D);

	m_polyline.setVertexes(vertexes2DNew);
	m_dirty = true;
	vertexes();

	return offset2D;
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
	FUNCID(Polygon3D::boundingBox);
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
	FUNCID(Polygon3D::enlargeBoundingBox);
	if (!isValid())
		throw IBK::Exception("Invalid polygon.", FUNC_ID);
	// Note: do not access m_vertexes directly, as this array may be dirty
	const std::vector<Vector3D> & points = vertexes();
	for (const IBKMK::Vector3D & v: points)
		IBKMK::enlargeBoundingBox(v, lowerValues, upperValues);
}



// *** PRIVATE MEMBER FUNCTIONS ***

bool Polygon3D::smallerVectZero(const IBKMK::Vector3D& vect) {
	if (vect.m_x < 0)
		return true;
	if (vect.m_x > 0)
		return false;
	if (vect.m_y < 0)
		return true;
	if (vect.m_y > 0)
		return false;
	if (vect.m_z < 0)
		return true;
	if (vect.m_z > 0)
		return false;
	return false;
}

IBKMK::Vector3D Polygon3D::computeNormal(const std::vector<IBKMK::Vector3D>& polygon) {
	FUNCID(Polygon3D::computeNormal);

	if (polygon.size() < 3)
		return IBKMK::Vector3D(1,0,0);

	IBKMK::Vector3D n(0,0,0);
	IBKMK::Vector3D e(0,0,0);

	for(unsigned int i=0; i<polygon.size(); ++i)
		e += polygon[i];

	e /= polygon.size();


	for(unsigned int i=0; i<polygon.size(); ++i) {
		unsigned int s = polygon.size();
		unsigned int j = (i + s - 1)%s;

		IBKMK::Vector3D v1 = polygon[j] - e;
		IBKMK::Vector3D v2 = polygon[i] - e;

		n += v1.crossProduct(v2).normalized();
	}

	if (n.magnitudeSquared() < 0.01) {

		n = Vector3D(0,0,0);

		for(unsigned int i=0; i<polygon.size()-1; ++i) {
			unsigned int s = polygon.size();
			unsigned int j = (i + s - 1)%s;

			IBKMK::Vector3D v1 = polygon[j] - e;
			IBKMK::Vector3D v2 = polygon[i] - e;

			n += v1.crossProduct(v2).normalized();
		}

		if (n.magnitudeSquared() > 0.01)
			return n.normalized();

		IBK::IBK_Message(IBK::FormatString("Start point:\t%1\t%2\t%3")
						 .arg(e.m_x)
						 .arg(e.m_y)
						 .arg(e.m_z), IBK::MSG_ERROR);
		for (unsigned int i=0; i<polygon.size(); ++i) {

			unsigned int s = polygon.size();
			unsigned int j = (i + s -1)%s;

			IBKMK::Vector3D v1 = polygon[j] - e;
			IBKMK::Vector3D v2 = polygon[i] - e;

			Vector3D ntest = v1.crossProduct(v2).normalized();

			IBK::IBK_Message(IBK::FormatString("Poly point %4:\t%1\t%2\t%3\t\tNormal:\t%5\t%6\t%7")
							 .arg(polygon[i].m_x)
							 .arg(polygon[i].m_y)
							 .arg(polygon[i].m_z)
							 .arg(i)
							 .arg(ntest.m_x)
							 .arg(ntest.m_y)
							 .arg(ntest.m_z),IBK::MSG_ERROR);
		}

		throw IBK::Exception(IBK::FormatString("Could not determine normal of polygon 3D."), FUNC_ID);
	}

	return n.normalized();
}

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
	//	IBKMK::Vector3D y = verts.back() - verts[0];
	IBKMK::Vector3D n1 = computeNormal(verts);
	//	IBKMK::Vector3D n2;

	//	m_localX.crossProduct(y, n2);


	//	if(n1 != n2) {
	//		IBK::IBK_Message(IBK::FormatString("Normal 1 - X: %1 Y: %2 Z: %3")
	//						 .arg(n1.m_x).arg(n1.m_y).arg(n1.m_z), IBK::MSG_WARNING);

	//		IBK::IBK_Message(IBK::FormatString("Normal 2 - X: %1 Y: %2 Z: %3")
	//						 .arg(n2.m_x).arg(n2.m_y).arg(n2.m_z), IBK::MSG_WARNING);
	//	}

	// if we interpret n as area between y and localX vectors, this should
	// be reasonably large (> 1 mm2). If we, however, have a very small magnitude
	// the vectors y and localX are (nearly) collinear, which should have been prevented by
	// eliminateColliniarPoints() before.
	if (n1.magnitude() < 1e-9)
		return; // invalid vertex input
	//n2.normalize();

	/* das ist alt und kann weg da die richtung der normalen nicht immer richtig ist.
	 * das wird an anderer stelle entschieden

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
	*/

	// save-guard against degenerate polygons (i.e. all points close to each other or whatever error may cause
	// the normal vector to have near zero magnitude... this may happen for extremely small polygons, when
	// the x and y vector lengths are less than 1 mm in length).
	m_normal = n1;
	// now compute local Y axis
	n1.crossProduct(m_localX, m_localY);
	// normalize localX and localY
	m_localX.normalize();
	m_localY.normalize();
	// store first point as offset
	m_offset = verts[0];
}


void Polygon3D::update2DPolyline(const std::vector<Vector3D> & verts) {
	// NOTE: DO NOT ACCESS m_vertexes IN THIS FUNCTION!

	m_polyline.clear();
	IBK_ASSERT(verts.size() >= 3);

	std::vector<IBKMK::Vector2D> poly;
	poly.reserve(verts.size());

	// first point is v0 = origin
	poly.push_back( IBKMK::Vector2D(0,0) );
	const IBKMK::Vector3D & offset = verts[0];

	// now process all other points
	for (unsigned int i=1; i<verts.size(); ++i) {
		const IBKMK::Vector3D & v = verts[i];
		double x,y;
		if (IBKMK::planeCoordinates(offset, m_localX, m_localY, v, x, y)) {
			poly.push_back( IBKMK::Vector2D(x,y) );
		}
		else {
			return;
		}
	}
	// set polygon in polyline
	// Mind: this may lead to removal of points if two are close together
	//       and thus also cause the polygon to be invalid
	m_polyline.setVertexes(poly);
	bool valid = m_polyline.isValid() && m_polyline.isSimplePolygon();

	// check if normal is right areaSigned >= 0 than ok
	// if not calculate a new one ... also calculate new localY
	if(valid){
		double areaS2 = m_polyline.areaSigned(2);

		if(areaS2 <0){
			m_normal *= -1;
			m_localX.crossProduct(m_normal, m_localY);
		}
	}

}


bool Polygon3D::dividePolyCycles(std::vector<Vector3D> & verts, const IBKMK::Vector3D trimPlaneNormal,
								 const double offset, std::vector<std::vector<Vector3D>> & outputVerts) const {

	if (verts.size() > 3) {

		// we test for edges on the trimPlane that are contained within each other, which means the polygon needs to be divided
		for (unsigned int i = 0, count = verts.size(); i<count; ++i ) {
			// j+3 because we dont need to match the same edge, neither the neighbouring edges
			for (unsigned int j = 0, count = verts.size(); (j+3)<count; ++j ) {

				// the vertex indices of the edges to be matched for containment
				int indexIStart =  i;
				int indexIEnd =   (i+1)   % verts.size();
				int indexJStart = (j+i+2) % verts.size();
				int indexJEnd =   (j+i+3) % verts.size();

				IBKMK::Vector3D iStart = verts[indexIStart];
				IBKMK::Vector3D iEnd =   verts[indexIEnd];
				IBKMK::Vector3D jStart = verts[indexJStart];
				IBKMK::Vector3D jEnd =   verts[indexJEnd];

				// test if edge i lies on the trimPlane
				if (IBK::nearly_equal<5>(trimPlaneNormal.scalarProduct(iStart)-offset,0)
						&& IBK::nearly_equal<5>(trimPlaneNormal.scalarProduct(iEnd)-offset,0)) {

					// test if first vertex of edge j is contained within edge i
					// near equal is necessary because the edges might be constant regarding 1 or 2 coordinates, and won't be "contained" in all 3 dimensions
					if (pointBetweenPoints(jStart, iStart, iEnd) && IBK::nearly_equal<5>(trimPlaneNormal.scalarProduct(jStart)-offset,0)) {

						// test if second vertex of edge j is contained within edge i
						if (pointBetweenPoints(jEnd, iStart, iEnd) && IBK::nearly_equal<5>(trimPlaneNormal.scalarProduct(jEnd)-offset,0)) {
							//     _    _
							//  __|_|__|_|__

							std::vector<Vector3D> subPolyA = {};
							std::vector<Vector3D> subPolyB = {};

							// we create two new polygons:
							// iEnd...jStart and jEnd...iStart

							for (int k = 0, count = ((indexJStart - indexIEnd + verts.size())%verts.size())+1; k<count; ++k ) {
								subPolyA.push_back(verts[(indexIEnd + k)%verts.size()]);
							}

							for (int k = 0, count = ((indexIStart - indexJEnd + verts.size())%verts.size())+1; k<count; ++k ) {
								subPolyB.push_back(verts[(indexJEnd + k)%verts.size()]);
							}

							// recursive cycle divison on resulting polygons

							std::vector<std::vector<Vector3D>> outputVertsA = {};
							std::vector<std::vector<Vector3D>> outputVertsB = {};
							if (dividePolyCycles(subPolyA, trimPlaneNormal, offset, outputVertsA)) {
								for (const std::vector<Vector3D> & verts : outputVertsA) {
									outputVerts.push_back(verts);
								}
							} else {
								outputVerts.push_back(subPolyA);
							}

							if (dividePolyCycles(subPolyB, trimPlaneNormal, offset, outputVertsB)) {
								for (const std::vector<Vector3D> & verts : outputVertsB) {
									outputVerts.push_back(verts);
								}
							} else {
								outputVerts.push_back(subPolyB);
							}

							return true;

						} else {
							// ___/\/\___ where point jStart divides edge i
							// mark current polygon for removal

							// append new polygons after recursive processing
							std::vector<Vector3D> subPolyA = {};
							std::vector<Vector3D> subPolyB = {};

							// we create two new polygons:
							// iEnd...jStart and jStart...iStart
							for (int k = 0, count = ((indexJStart - indexIEnd + verts.size())%verts.size())+1; k<count; ++k ) {
								subPolyA.push_back(verts[(indexIEnd + k)%verts.size()]);
							}

							for (int k = 0, count = ((indexIStart - indexJStart + verts.size())%verts.size())+1; k<count; ++k ) {
								subPolyB.push_back(verts[(indexJStart + k)%verts.size()]);
							}

							// recursive cycle divison on resulting polygons
							std::vector<std::vector<Vector3D>> outputVertsA = {};
							std::vector<std::vector<Vector3D>> outputVertsB = {};

							if (dividePolyCycles(subPolyA, trimPlaneNormal, offset, outputVertsA)) {
								for (const std::vector<Vector3D> & verts : outputVertsA) {
									outputVerts.push_back(verts);
								}
							} else {
								outputVerts.push_back(subPolyA);
							}

							if (dividePolyCycles(subPolyB, trimPlaneNormal, offset, outputVertsB)) {
								for (const std::vector<Vector3D> & verts : outputVertsB) {
									outputVerts.push_back(verts);
								}
							} else {
								outputVerts.push_back(subPolyB);
							}

							return true;
						}
					}
				}
			}
		}
	}
	return false;
}


void Polygon3D::polyCyclesAfterTrimming(std::vector<IBKMK::Polygon3D> &polys,
										const IBKMK::Vector3D &trimPlaneNormal,
										const double offset) const {

	std::vector<int> indicesToBeRemovedAfter;
	for (unsigned int polysIterator = 0, count = polys.size(); polysIterator<count; ++polysIterator ) {
		std::vector<std::vector<Vector3D>> outputVerts;
		std::vector<Vector3D> verts;
		try {
			verts = polys[polysIterator].vertexes();
		}
		catch (IBK::Exception &ex) {
			IBK::IBK_Message("Vertexes of polygon after trimming are empty. Skipping.");
			continue;
		}

		if (!dividePolyCycles(verts, trimPlaneNormal, offset, outputVerts))
			continue;

		// dividePolyCycles returned true, therefore remove "before" state
		indicesToBeRemovedAfter.push_back(polysIterator);

		// Add back polygon
		for (const std::vector<Vector3D> &vertsOut : outputVerts) {
			polys.push_back(vertsOut);
		}
	}

	// reverse indices so we don't interfere with the iterator
	// remove polygons
	for (unsigned int i = indicesToBeRemovedAfter.size(); i > 0; --i) {
		polys.erase(polys.begin() + indicesToBeRemovedAfter.at(i-1));
	}


}


bool Polygon3D::trimByPlane(const IBKMK::Polygon3D &plane, std::vector<IBKMK::Polygon3D> &trimmedPolygons) const {
	const std::vector<Vector3D> &vertsA = vertexes();
	const std::vector<Vector3D> &vertsB = plane.vertexes();

	IBK_ASSERT(vertsA.size() > 2);
	IBK_ASSERT(plane.vertexes().size() > 2);

	int vertsSize = vertsA.size();

	// get arbitrary base vectors for polygon A and B's planes
	// compensating for different sizes of span vectors to have later calculations in the same order of magnitude
	IBKMK::Vector3D vectorA1 = vertsA[1] - vertsA[0];
	vectorA1 = vectorA1 * (10/vectorA1.magnitude());
	IBKMK::Vector3D vectorA2 = vertsA[2] - vertsA[0];
	vectorA2 = vectorA2 * (10/vectorA2.magnitude());

	IBKMK::Vector3D vectorB1 = vertsB[1] - vertsB[0];
	vectorB1 = vectorB1 * (10/vectorB1.magnitude());
	IBKMK::Vector3D vectorB2 = vertsB[2] - vertsB[0];
	vectorB2 = vectorB2 * (10/vectorB2.magnitude());

	// use cross product to obtain normal vectors
	const IBKMK::Vector3D normalVectorA = vectorA1.crossProduct(vectorA2);
	const IBKMK::Vector3D normalVectorB = vectorB1.crossProduct(vectorB2);

	const int offsetB = normalVectorB.scalarProduct(vertsB[0]);

	// check if polygon planes A & B are parallel
	// if crossProduct of normal vectors returns 0-vector then normal vectors are parallel
	// magnitude of normal vector will quickly exceed 1e+2 for small rotations, so 1e-1 check is suited
	if (IBK::nearly_equal<1>(normalVectorA.crossProduct(normalVectorB).magnitude(), 0)) {
		// planes are parallel
		IBK::IBK_Message("Trimming plane is coplanar", IBK::MSG_ERROR);
		return false;
	} else {
		// planes intersect
		// iterate over all vertices in A
		// create a vector of vertex locations -1 / 0 / 1 depending on the vertex's side of the trimming plane

		std::vector<int> vertsAsorted = {};
		double distVertToPlane;

		for (unsigned int i = 0, count = vertsSize; i<count; ++i ) {
			distVertToPlane = normalVectorB.scalarProduct(vertsA[i])-offsetB;
			if (IBK::nearly_equal<5>(distVertToPlane, 0)) {
				vertsAsorted.push_back(0);
			} else if (distVertToPlane > 0) {
				vertsAsorted.push_back(1);
			} else /* distVertToPlane < 0 */ {
				vertsAsorted.push_back(-1);
			}

		}

		// sort vertices by the side of the trimming plane they're located on
		std::vector<IBKMK::Vector3D> vertsPos = {};
		std::vector<IBKMK::Vector3D> vertsNeg = {};
		IBKMK::Vector3D edgeA;
		IBKMK::Vector3D pointOfIntersection;
		double r = 0;

		for (unsigned int i = 0, count = vertsSize; i<count; ++i ) {
			// if vertex lies on plane
			if (vertsAsorted[i] == 0) {
				// if predecessor and successor are on different sides: add vertex to both halves
				if (vertsAsorted[(i-1+vertsSize)%vertsSize] * vertsAsorted[(i+1)%vertsSize] == -1) {
					vertsPos.push_back(vertsA[i]);
					vertsNeg.push_back(vertsA[i]);

					// else if they are on the same side
				} else if (vertsAsorted[(i-1+vertsSize)%vertsSize] * vertsAsorted[(i+1)%vertsSize] == 1) {
					if (vertsAsorted[(i+1)%vertsSize] == 1) {
						vertsPos.push_back(vertsA[i]);
					} else /* vertsAsorted[i+1] == -1 */ {
						vertsNeg.push_back(vertsA[i]);
					}
				} else {
					// one of the neighbouring vertices lies on the intersection plane too, so only add vertex to one side.
					// if both neighboring vertices lie on plane, the polygon is malformatted. This case is not regarded here anymore.
					if (vertsAsorted[(i-1+vertsSize)%vertsSize] == 1 || vertsAsorted[(i+1)%vertsSize] == 1) {
						// either one of the neighbouring points is on the positive side
						vertsPos.push_back(vertsA[i]);
					} else {
						vertsNeg.push_back(vertsA[i]);
					}
				}
			} else {
				// vertex is not on the plane
				if (vertsAsorted[i] == 1) {
					vertsPos.push_back(vertsA[i]);
				} else /* vertsAsorted[i] == -1 */ {
					vertsNeg.push_back(vertsA[i]);
				}

				// if the next vertex lies on the other trim-plane side
				if (vertsAsorted[i] * vertsAsorted[(i+1)%vertsSize] == -1) {
					// add intersection point to both halves
					edgeA = vertsA[(i+1)%vertsSize] - vertsA[i];
					// calculating point of intersection
					// ...by inserting edgeA into plane B to get factor r, for our edge equation
					// equation: normalVectorB * (vertsA[i] + r * edgeA) == offsetB;
					// division by zero can't occure because the end points lie on different plane sides -> edgeA not coplanar
					r = (offsetB - normalVectorB.scalarProduct(vertsA[i])) / normalVectorB.scalarProduct(edgeA);
					// get point of intersection:
					pointOfIntersection = vertsA[i] + r * edgeA;
					vertsPos.push_back(pointOfIntersection);
					vertsNeg.push_back(pointOfIntersection);
				} // otherwise the next one is either on the same side or on the plane (do nothing yet in this iteration)
			}
		}

		if (vertsPos.size() == 0 || vertsNeg.size() == 0) {
			IBK::IBK_Message("Plane does not intersect polygon", IBK::MSG_ERROR);

			return false;

		}
		else {
			// we need to detect if Pos / Neg side is divided into multiple polygons
			std::vector<IBKMK::Polygon3D> tempPolygons;
			tempPolygons.push_back(vertsPos);
			tempPolygons.push_back(vertsNeg);

			polyCyclesAfterTrimming(tempPolygons, normalVectorB, offsetB);

			for (const IBKMK::Polygon3D &polygon : tempPolygons)
				trimmedPolygons.push_back(polygon);

			return true;
		}
	}
}

bool Polygon3D::mergeWithPolygon(const IBKMK::Polygon3D & polyB, std::vector<std::vector<Vector2D>> & holes, bool mergeOverlapping) {
	const char * const FUNC_ID = "[Polygon3D::mergeWithPolygon]";

	IBK_ASSERT(vertexes().size() > 2);
	IBK_ASSERT(polyB.vertexes().size() > 2);

	if (!IBK::near_zero(normal().scalarProduct(polyB.vertexes().at(1)-polyB.vertexes().at(0)))) return false;
	// we can now assume polygons to be (nearly) parallel

	double distPolyBToPlane = normal().scalarProduct(polyB.vertexes().at(0))-normal().scalarProduct(centerPoint());
	if (!IBK::near_zero(distPolyBToPlane)) return false;
	// we can now assume polygons to be (nearly) coplanar

	// detect overlap
	std::vector<Vector2D> polyA2D;
	polyA2D.reserve(vertexes().size());
	std::vector<Vector2D> polyB2D;
	polyB2D.reserve(polyB.vertexes().size());
	double x,y;
	for (const Vector3D & vertA : vertexes()) {
		IBKMK::planeCoordinates(offset(), localX(), localY(), vertA, x, y);
		polyA2D.push_back(IBKMK::Vector2D(x,y));
	}
	for (const Vector3D & vertB : polyB.vertexes()) {
		IBKMK::planeCoordinates(offset(), localX(), localY(), vertB, x, y);
		polyB2D.push_back(IBKMK::Vector2D(x,y));
	}

	bool polyIntersect = polyIntersect2D(polyA2D, polyB2D, true);
	if (polyIntersect) {
		if (!mergeOverlapping) throw IBK::Exception(IBK::FormatString("Overlapping polygons with mergeOverlapping=false."), FUNC_ID);

		// add intersection points to both polygons
		IBKMK::Vector2D intersectionPoint;
		unsigned int i = 0;
		while (i < polyA2D.size()) {
			const Vector2D & p1 = polyA2D.at(i);
			const Vector2D & p2 = polyA2D.at((i+1)%polyA2D.size());
			// This small offset vector is necessary, otherwise we would just find the same intersection again instead of multiple ones
			Vector2D diffVec = p2-p1;
			diffVec /= 1e8;
			// if intersects and if neither of the points happens to be the intersection point
			if (intersectsLine2D(polyB2D, p1+diffVec, p2-diffVec, intersectionPoint) &&
				!IBK::near_zero((intersectionPoint - p1).magnitude()) &&
				!IBK::near_zero((intersectionPoint - p2).magnitude()) &&
				std::find(polyA2D.begin(), polyA2D.end(), intersectionPoint) == polyA2D.end()) {

				polyA2D.insert(polyA2D.begin() + i + 1, intersectionPoint);
			} else
				++i;
		}

		i = 0;
		while (i < polyB2D.size()) {
			const Vector2D & p1 = polyB2D.at(i);
			const Vector2D & p2 = polyB2D.at((i+1)%polyB2D.size());
			Vector2D diffVec = p2-p1;
			diffVec /= 1e8;
			if (intersectsLine2D(polyA2D, p1+diffVec, p2-diffVec, intersectionPoint) &&
				!IBK::near_zero((intersectionPoint - p1).magnitude()) &&
				!IBK::near_zero((intersectionPoint - p2).magnitude()) &&
				std::find(polyB2D.begin(), polyB2D.end(), intersectionPoint) == polyB2D.end()) {

				polyB2D.insert(polyB2D.begin() + i + 1, intersectionPoint);
			} else
				++i;
		}

		// Now both polygons contain their outline as well as every intersection point with the other polygon
		// create a mapping between shared vertices
		std::set<unsigned int> commonA, commonB;
		for (unsigned int j = 0; j<polyA2D.size(); ++j ) {
			const Vector2D & vertA = polyA2D.at(j);
			for (unsigned int k = 0; k<polyB2D.size(); ++k ) {
				const Vector2D & vertB = polyB2D.at(k);
				if (IBK::near_zero((vertA-vertB).magnitude())) {
					commonA.insert(j);
					commonB.insert(k);
					break;
				}
			}
		}

		// if no intersections, one polygon is contained within the other
		if (commonA.size() == 0) {
			if (pointInPolygon(polyB2D, polyA2D.front()) == 1) {
				setVertexes(polyB.vertexes());
			}
			return true;
		}

		// remove all vertices that are located within the other polygon
		for (int j = polyA2D.size()-1; j>=0; --j ) {
			if (commonA.find(j) != commonA.end()) continue;
			const Vector2D & vertA = polyA2D.at(j);
			if (pointInPolygon(polyB2D, vertA) == 1) polyA2D.erase(polyA2D.begin() + j);
		}
		for (int k = polyB2D.size()-1; k>=0; --k ) {
			if (commonB.find(k) != commonB.end()) continue;
			const Vector2D & vertB = polyB2D.at(k);
			if (pointInPolygon(polyA2D, vertB) == 1) polyB2D.erase(polyB2D.begin() + k);
		}
		// we're left with shared vertices and verts that lie outside of the respective other polygon

	} else {
		unsigned int i;
		for (i = 0; i<polyA2D.size(); ++i ) {
			const Vector2D vertA1 = polyA2D.at(i);
			const Vector2D vertA2 = polyA2D.at((i+1)%polyA2D.size());
			unsigned int j;
			for (j = 0; j<polyB2D.size(); ++j) {
				const Vector2D vertB1 = polyB2D.at(j);
				const Vector2D vertB2 = polyB2D.at((j+1)%polyB2D.size());

				// if scalar product of edge B and orthogonal of edge A is near_zero -> vectors are in parallel
				if (IBK::near_zero((vertB2-vertB1).scalarProduct(Vector2D((vertA2-vertA1).m_y,-(vertA2-vertA1).m_x)))) {
					// vectors parallel, test if vectors in near_zero distance from each other
					if (IBK::near_zero(lineToPointDistance2D(vertA1, vertB1, vertB2)) &&
							IBK::near_zero(lineToPointDistance2D(vertA2, vertB1, vertB2)) &&
							IBK::near_zero(lineToPointDistance2D(vertB1, vertA1, vertA2)) &&
							IBK::near_zero(lineToPointDistance2D(vertB2, vertA1, vertA2))) {

						// test if shared verts or inbetween
						bool a1inB = pointBetweenPoints2D(vertA1, vertB1, vertB2);
						bool a2inB = pointBetweenPoints2D(vertA2, vertB1, vertB2);
						bool b1inA = pointBetweenPoints2D(vertB1, vertA1, vertA2);
						bool b2inA = pointBetweenPoints2D(vertB2, vertA1, vertA2);
						if (!a1inB && !a2inB && !b1inA && !b2inA) continue; // no overlap
						// we now have some type of overlap / shared edge
						if (a1inB && a2inB && b1inA && b2inA) continue; // both verts shared, no need for additional points

						if ((a1inB != a2inB) && (b1inA != b2inA)) { // partial overlap of the two edges
							// firstly make sure to exclude the case that they share a vert:
							if (a1inB) {
								if (b1inA) {
									if (IBK::near_zero((vertA1-vertB1).magnitude())) continue;
								} else {
									if (IBK::near_zero((vertA1-vertB2).magnitude())) continue;
								}
							} else {
								if (b1inA) {
									if (IBK::near_zero((vertA2-vertB1).magnitude())) continue;
								} else {
									if (IBK::near_zero((vertA2-vertB2).magnitude())) continue;
								}
							}
							// the really partially overlap
							if (a1inB) {
								polyB2D.insert(polyB2D.begin() + j + 1, vertA1);
							} else /*a2inB*/ {
								polyB2D.insert(polyB2D.begin() + j + 1, vertA2);
							}
							if (b1inA) {
								polyA2D.insert(polyA2D.begin() + i + 1, vertB1);
							} else /*b2inA*/ {
								polyA2D.insert(polyA2D.begin() + i + 1, vertB2);
							}
							++j;
							++i;

						} else if (a1inB && a2inB && !b1inA && !b2inA) { // a contained in b
							bool flipVertDir = (vertB1 - vertA2).magnitude() < (vertB1 - vertA1).magnitude();
							if (flipVertDir) {
								polyB2D.insert(polyB2D.begin() + j + 1, vertA1);
								polyB2D.insert(polyB2D.begin() + j + 1, vertA2);
							} else {
								polyB2D.insert(polyB2D.begin() + j + 1, vertA2);
								polyB2D.insert(polyB2D.begin() + j + 1, vertA1);
							}
							j += 2;

						} else if (!a1inB && !a2inB && b1inA && b2inA) { // b contained in a
							bool flipVertDir = (vertA1 - vertB2).magnitude() < (vertA1 - vertB1).magnitude();
							if (flipVertDir) {
								polyA2D.insert(polyA2D.begin() + i + 1, vertB1);
								polyA2D.insert(polyA2D.begin() + i + 1, vertB2);
							} else {
								polyA2D.insert(polyA2D.begin() + i + 1, vertB2);
								polyA2D.insert(polyA2D.begin() + i + 1, vertB1);
							}
							i += 2;

						} else if (a1inB != a2inB) { // b contained in a with one shared vertex
						// "a1inB!=a2inB && b1inA!=b2inA" has been tested prior, and "a1inB!=a2inB && !b1inA&&!b2inA" is geometrically impossible, therefore both bInA true
							if (a1inB) {
								if (IBK::near_zero((vertA1 - vertB1).magnitude())) {
									polyA2D.insert(polyA2D.begin() + i + 1, vertB2);
								} else {
									polyA2D.insert(polyA2D.begin() + i + 1, vertB1);
								}
							} else /* a2inB */ {
								if (IBK::near_zero((vertA2 - vertB1).magnitude())) {
									polyA2D.insert(polyA2D.begin() + i + 1, vertB2);
								} else {
									polyA2D.insert(polyA2D.begin() + i + 1, vertB1);
								}
							}
							++i;

						} else if (b1inA != b2inA) { // a contained in b with one shared vertex
							if (b1inA) {
								if (IBK::near_zero((vertB1 - vertA1).magnitude())) {
									polyB2D.insert(polyB2D.begin() + j + 1, vertA2);
								} else {
									polyB2D.insert(polyB2D.begin() + j + 1, vertA1);
								}
							} else /* b2inA */ {
								if (IBK::near_zero((vertB2 - vertA1).magnitude())) {
									polyB2D.insert(polyB2D.begin() + j + 1, vertA2);
								} else {
									polyB2D.insert(polyB2D.begin() + j + 1, vertA1);
								}
							}
							++j;
						}
					}
				}
			}
		}
		// now all overlapping edges have received all necessary shared verts
	}

	// We now treat both cases (overlap and shared edges) with the same algorithm, as the "shared edges" case is actually a possible outcome of our overlap treatment

	if (polyB2D.size() < 3) {
		return true;
	} else if (polyA2D.size() < 3) {
		setVertexes(polyB.vertexes());
		return true;
	}

	// test if both polygons turn in different directions
	if (polyB.normal().scalarProduct(normal()) < 0) {
		std::reverse(polyB2D.begin(), polyB2D.end());
	}

	// create new common-verts mapping after deletion
	std::map<unsigned int, unsigned int> commonAToB;
	std::map<unsigned int, unsigned int> commonBToA;
	for (unsigned int j = 0; j<polyA2D.size(); ++j ) {
		const Vector2D & vertA = polyA2D.at(j);
		for (unsigned int k = 0; k<polyB2D.size(); ++k ) {
			const Vector2D & vertB = polyB2D.at(k);
			if (IBK::near_zero((vertA-vertB).magnitude())) {
				commonAToB[j] = k;
				commonBToA[k] = j;
				break;
			}
		}
	}

	// If non-overlapping we didn't check if polygons do even touch, therefore return false if no common verts detected
	if (commonAToB.size() < 2) return false;

	std::vector<std::vector<Vector2D>> newShapes;
	std::set<Vector2D> discovered;
	bool foundNew = true;

	unsigned int startIndex;
	unsigned int currentIndex;

	bool startInA = true;
	bool currentlyInA = true;

	while (foundNew) {
		foundNew = false;

		// Try to discover a new polygon within the shape -> find a vert that is not shared and not previously discovered
		for (unsigned int i = 0; i<polyA2D.size(); ++i ) {
			if (commonAToB.find(i) == commonAToB.end()
					&& discovered.find(polyA2D.at(i)) == discovered.end()) {
				foundNew = true;
				startIndex = i;
				break;
			}
		}
		if (foundNew) {
			startInA = true;
		} else {
			// search in polyB for an isolated node
			for (unsigned int j = 0; j<polyB2D.size(); ++j ) {
				if (commonBToA.find(j) == commonBToA.end()
						&& discovered.find(polyB2D.at(j)) == discovered.end()) {
					foundNew = true;
					startIndex = j;
					break;
				}
			}
			if (foundNew) {
				startInA = false;
			}
		}

		if (foundNew) {
			// run shape explore algorithm
			std::vector<Vector2D> newPoly;

			currentIndex = startIndex;
			currentlyInA = startInA;

			unsigned int stepsLimit = polyA2D.size() + polyB2D.size(); // to prevent infinite loops on errors

			for (unsigned int k = 0; k<stepsLimit; ++k) {
				if (currentlyInA) {
					newPoly.push_back(polyA2D.at(currentIndex));
					discovered.insert(polyA2D.at(currentIndex));

					if (commonAToB.find(currentIndex) != commonAToB.end() && commonAToB.find((currentIndex+1)%polyA2D.size()) != commonAToB.end()) {
						currentlyInA = false;
						currentIndex = (commonAToB[currentIndex]+ 1) % polyB2D.size();
					} else
						currentIndex = (currentIndex + 1) % polyA2D.size();
				} else {
					newPoly.push_back(polyB2D.at(currentIndex));
					discovered.insert(polyB2D.at(currentIndex));

					if (commonBToA.find(currentIndex) != commonBToA.end() && commonBToA.find((currentIndex+1)%polyB2D.size()) != commonBToA.end()) {
						currentlyInA = true;
						currentIndex = (commonBToA[currentIndex] + 1) % polyA2D.size();
					} else
						currentIndex = (currentIndex + 1) % polyB2D.size();
				}

				if (currentIndex == startIndex && currentlyInA == startInA) break;
			}
			newShapes.push_back(newPoly);

		}
	}

	// Transform merge result polygons back to 3D
	std::vector<Vector2D> polyline2D = newShapes.front();
	if (newShapes.size() > 1) {
		// sort holes and polyline
		for (unsigned int pl = 1; pl<newShapes.size(); ++pl ) {
			std::vector<Vector2D> & poly2DB = newShapes.at(pl);

			bool aOutsideB = false;
			bool bOutsideA = false;

			for (Vector2D vert : polyline2D) {
				if (pointInPolygon(poly2DB, vert) == -1) {
					aOutsideB = true;
					break;
				}
			}
			for (Vector2D vert : poly2DB) {
				if (pointInPolygon(polyline2D, vert) == -1) {
					bOutsideA = true;
					break;
				}
			}

			if (aOutsideB) {
				holes.push_back(poly2DB);
			} else if (bOutsideA) {
				holes.push_back(polyline2D);
				polyline2D = newShapes.at(pl);
			}
		}
	}

	std::vector<IBKMK::Vector3D> verts3D(polyline2D.size());
	for (unsigned int i = 0; i < polyline2D.size(); ++i) {
		verts3D[i] = offset() + localX() * polyline2D[i].m_x + localY() * polyline2D[i].m_y;
	}
	setVertexes(verts3D);

	return true;
}


} // namespace IBKMK

