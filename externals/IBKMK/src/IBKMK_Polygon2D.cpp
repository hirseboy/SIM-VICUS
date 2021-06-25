#include "IBKMK_Polygon2D.h"

#include <set>

#include <IBK_Line.h>
#include <IBK_math.h>
#include <IBK_messages.h>

#include "IBKMK_2DCalculations.h"

namespace IBKMK {

Polygon2D::Polygon2D(const std::vector<IBKMK::Vector2D> & vertexes) {
	setVertexes(vertexes);
}




// Comparison operator !=
bool Polygon2D::operator!=(const Polygon2D &other) const {
	if (m_type != other.m_type)
		return true;
	if (m_vertexes != other.m_vertexes)
		return true;
	return false;
}


void Polygon2D::addVertex(const IBK::point2D<double> & v) {
	m_vertexes.push_back(v);
	checkPolygon(); // if we have a triangle/rectangle, this is detected here
}


void Polygon2D::removeVertex(unsigned int idx){
	FUNCID(Polygon2D::removeVertex);
	if (idx >= (unsigned int)m_vertexes.size())
		throw IBK::Exception(IBK::FormatString("Index %1 out of range (vertex count = %2).").arg(idx).arg(m_vertexes.size()), FUNC_ID);
	m_vertexes.erase(m_vertexes.begin()+idx);
	m_type = T_Polygon; // assume the worst
	checkPolygon(); // if we have a triangle/rectangle, this is detected here
}


void Polygon2D::checkPolygon() {
	m_valid = false;
	if (m_vertexes.size() < 3)
		return;
	eleminateColinearPts();

	// try to simplify polygon to internal rectangle/parallelogram definition
	// this may change m_type to Rectangle or Triangle and subsequently speed up operations
	detectType();
	// we need 3 vertexes (not collinear) to continue
	if (m_vertexes.size() < 3)
		return;

	// polygon must not be winding into itself, otherwise triangulation would not be meaningful
	m_valid = isSimplePolygon();
}


void Polygon2D::flip() {
	std::vector<IBKMK::Vector2D>(m_vertexes.rbegin(), m_vertexes.rend()).swap(m_vertexes);
}


bool Polygon2D::intersectsLine2D(const IBK::point2D<double> &p1, const IBK::point2D<double> &p2,
								 IBK::point2D<double> & intersectionPoint) const{
	return IBKMK::intersectsLine2D(m_vertexes, p1, p2, intersectionPoint);
}


void Polygon2D::setVertexes(const std::vector<IBKMK::Vector2D> & vertexes) {
	m_vertexes = vertexes;
	checkPolygon(); // if we have a triangle/rectangle, this is detected here
}


double Polygon2D::area() const {
	double surfArea=0;
	unsigned int sizeV = m_vertexes.size();
	for (unsigned int i=0; i<sizeV; ++i){
		const IBKMK::Vector2D &p0 = m_vertexes[i];
		const IBKMK::Vector2D &p1 = m_vertexes[(i+1)%sizeV];
		const IBKMK::Vector2D &p2 = m_vertexes[(i+2)%sizeV];
		surfArea += p1.m_x * (p2.m_y - p0.m_y);
	}
	surfArea *= 0.5;
	return surfArea;
}


double Polygon2D::circumference() const {
	double circumference=0;
	unsigned int sizeV = m_vertexes.size();
	for (unsigned int i=0; i<sizeV; ++i){
		const IBKMK::Vector2D &p0 = m_vertexes[i];
		const IBKMK::Vector2D &p1 = m_vertexes[(i+1)%sizeV];
		circumference += (p1-p0).magnitude();
	}
	return circumference;
}


void Polygon2D::detectType() {
	m_type = T_Polygon;
	if (m_vertexes.size() == 3) {
		m_type = T_Triangle;
		return;
	}
	if (m_vertexes.size() != 4)
		return;
	const IBKMK::Vector2D & a = m_vertexes[0];
	const IBKMK::Vector2D & b = m_vertexes[1];
	const IBKMK::Vector2D & c = m_vertexes[2];
	const IBKMK::Vector2D & d = m_vertexes[3];
	IBKMK::Vector2D c2 = b + (d-a);
	c2 -= c;
	// we assume we have zero length for an rectangle
	if (c2.magnitude() < 1e-4)  // TODO : should this be a relative error? suppose we have a polygon of size 1 mm x 1 mm, then any polygon will be a rectangle
		m_type = T_Rectangle;
}


bool Polygon2D::isSimplePolygon() const {
	std::vector<IBK::Line>	lines;
	for (unsigned int i=0, vertexCount = m_vertexes.size(); i<vertexCount; ++i) {
		lines.emplace_back(
					IBK::Line(
					IBK::point2D<double>(
								  m_vertexes[i].m_x,
								  m_vertexes[i].m_y),
					IBK::point2D<double>(
								  m_vertexes[(i+1) % vertexCount].m_x,
								  m_vertexes[(i+1) % vertexCount].m_y))
				);
	}
	if (lines.size()<4)
		return true;
	for (unsigned int i=0; i<lines.size();++i) {
		for (unsigned int j=0; j<lines.size()-2; ++j) {
			unsigned int k1 = (i+1)%lines.size();
			unsigned int k2 = (i-1);
			if(i==0)
				k2 = lines.size()-1;
			if(i==j || k1 == j || k2 == j )
				continue;
			//int k = (i+j+2)%lines.size();
			IBK::point2D<double> p;
			if (lines[i].intersects(lines[j], p))
				return false;
		}
	}

	return true;
}


void Polygon2D::eleminateColinearPts() {

	unsigned int vertexCount = m_vertexes.size();
	std::vector<unsigned int> erasePointIdx;
	for (unsigned int i=0; i<vertexCount; ++i){
		const IBKMK::Vector2D &p0 = m_vertexes[i];
		const IBKMK::Vector2D &p1 = m_vertexes[(i+1)%vertexCount];
		const IBKMK::Vector2D &p2 = m_vertexes[(i+2)%vertexCount];

		if(p1==p2){
			erasePointIdx.push_back((i+1)%vertexCount);
			continue;
		}

		double dx1 = p1.m_x - p0.m_x;
		double dx2 = p2.m_x - p0.m_x;
		double dy1 = p1.m_y - p0.m_y;
		double dy2 = p2.m_y - p0.m_y;

		if( IBK::near_zero(dx1) && IBK::near_zero(dx2)){
			//eleminate point
			erasePointIdx.push_back((i+1)%vertexCount);
			continue;
		}
		if(IBK::near_zero(dx1) || IBK::near_zero(dx2))
			continue;
		if( IBK::near_zero(std::abs(dy1/dx1) - std::abs(dy2/dx2))){
			//eleminate point
			erasePointIdx.push_back((i+1)%vertexCount);
			continue;
		}
	}

	unsigned int count=0;
	while (count<erasePointIdx.size()) {
		m_vertexes.erase(m_vertexes.begin()+erasePointIdx[count]);
		++count;
	}
}



} // namespace IBKMK

