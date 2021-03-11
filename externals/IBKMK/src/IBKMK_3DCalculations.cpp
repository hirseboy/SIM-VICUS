/*	IBK Math Kernel Library
		Copyright (c) 2001-2016, Institut fuer Bauklimatik, TU Dresden, Germany

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

#include "IBKMK_3DCalculations.h"

#include "IBKMK_Vector3D.h"

namespace IBKMK {


/* Solves equation system with Cramer's rule:
	 a x + c y = e
	 b x + d y = f
*/
static bool solve(double a, double b, double c,  double d,  double e,  double f, double & x, double & y) {
	double det = a*d - b*c;
	// Prevent division by very small numbers
	if (std::fabs(det) < 1e-7)
		return false;

	x = (e*d - c*f)/det;
	y = (a*f - e*b)/det;
	return true;
}


/* Computes the coordinates x, y of a point 'p' in a plane spanned by vectors a and b from a point 'offset', where rhs = p-offset.
	The computed plane coordinates are stored in variables x and y (the factors for vectors a and b, respectively).
	If no solution could be found (only possible if a and b are collinear or one of the vectors has length 0?),
	the function returns false.

	Note: when the point p is not in the plane, this function will still get a valid result.
*/
bool planeCoordinates(const IBKMK::Vector3D & offset, const IBKMK::Vector3D & a, const IBKMK::Vector3D & b,
							 const IBKMK::Vector3D & v, double & x, double & y)
{
	// We have 3 equations, but only two unknowns - so we have 3 different options to compute them.
	// Some of them may fail, so we try them all.

	const IBKMK::Vector3D & rhs = v-offset;
	// rows 1 and 2
	bool success = solve(a.m_x, a.m_y, b.m_x, b.m_y, rhs.m_x, rhs.m_y, x, y);
	if (!success)
		// rows 1 and 3
		success = solve(a.m_x, a.m_z, b.m_x, b.m_z, rhs.m_x, rhs.m_z, x, y);
	if (!success)
		// rows 2 and 3
		success = solve(a.m_y, a.m_z, b.m_y, b.m_z, rhs.m_y, rhs.m_z, x, y);
	if (!success)
		return false;

	// check that the point was indeed in the plane
	IBKMK::Vector3D v2 = offset + x*a + y*b;
	v2 -= v;
	if (v2.magnitude() > 1e-4)
		return false;
	return true;
}


double lineToPointDistance(const IBKMK::Vector3D & a, const IBKMK::Vector3D & d, const IBKMK::Vector3D & p,
												   double & lineFactor, IBKMK::Vector3D & p2)
{
		/// source: http://geomalgorithms.com/a02-_lines.html
		IBKMK::Vector3D v = ( p - a ); // from point to line p1

		double c1 = v.scalarProduct(d);
		double c2 = d.scalarProduct(d);

		lineFactor = c1 / c2;

		p2 = a + lineFactor * d;
		return (p2-p).magnitude();
}


double lineToLineDistance(const IBKMK::Vector3D & a1, const IBKMK::Vector3D & d1,
												  const IBKMK::Vector3D & a2, const IBKMK::Vector3D & d2,
												  double & l1, IBKMK::Vector3D & p1, double & l2)
{
	/// source: http://geomalgorithms.com/a02-_lines.html
	IBKMK::Vector3D v = a1 - a2;

	double d1Scalar = d1.scalarProduct(d1);// always >= 0
	double d1d2Scalar = d1.scalarProduct(d2);
	double d2Scalar = d2.scalarProduct(d2);

	double d1vScalar = d1.scalarProduct(v);// always >= 0
	double d2vScalar = d2.scalarProduct(v);

	double d = d1Scalar*d2Scalar - d1d2Scalar*d1d2Scalar;// always >= 0

	// compute the line parameters of the two closest points
	if (d<1E-4) {// the lines are almost parallel
			l1 = 0.0; // we have to set one factor to determine a point since there are infinite
			l2 = (d1d2Scalar>d2Scalar ? d1vScalar/d1d2Scalar : d2vScalar/d2Scalar);    // use the largest denominator
	}
	else {
			l1 = (d1d2Scalar*d2vScalar - d2Scalar*d1vScalar) / d;
			l2 = (d1Scalar*d2vScalar - d1d2Scalar*d1vScalar) / d;
	}

	p1 = a1 + ( l1 * d1 );				// point 1
	IBKMK::Vector3D p2 = a2 + (l2 * d2 ); // point 2

	// get the difference of the two closest points
	return ( p1 - p2 ).magnitude();   // return the closest distance
}


void pointProjectedOnPlane(const Vector3D & a, const Vector3D & normal, const Vector3D & p, Vector3D & projectedP) {
	// vector from point to plane origin
	projectedP = p-a;
	// scalar product between this vector and plane's normal
	double dist = normal.m_x*projectedP.m_x + normal.m_y*projectedP.m_y + normal.m_z*projectedP.m_z;
	// subtract normal vector's contribution of project P from p
	projectedP = p - dist*normal;
}

} // namespace IBKMK

