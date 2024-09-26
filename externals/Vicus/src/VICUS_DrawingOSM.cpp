#include "VICUS_DrawingOSM.h"
#include "VICUS_Constants.h"

#include "IBK_MessageHandler.h"
#include "IBK_messages.h"
#include "IBK_physics.h"

#include "NANDRAD_Utilities.h"

#include "IBKMK_2DCalculations.h"
#include "IBKMK_3DCalculations.h"

#include <QDebug>
#include "tinyxml.h"

/*! IBKMK::Vector3D to QVector3D conversion macro. */
inline QVector3D IBKVector2QVector(const IBKMK::Vector3D & v) {
	return QVector3D((float)v.m_x, (float)v.m_y, (float)v.m_z);
}

/*! QVector3D to IBKMK::Vector3D to conversion macro. */
inline IBKMK::Vector3D QVector2IBKVector(const QVector3D & v) {
	return IBKMK::Vector3D((double)v.x(), (double)v.y(), (double)v.z());
}

namespace VICUS {


std::vector<IBKMK::Vector2D> DrawingOSM::convertHoleToLocalCoordinates(
	const std::vector<IBKMK::Vector3D>& globalVertices,
	const IBKMK::Vector3D& offset,
	const IBKMK::Vector3D& localX,
	const IBKMK::Vector3D& localY) {
	std::vector<IBKMK::Vector2D> localVertices;
	localVertices.reserve(globalVertices.size());

	for (const IBKMK::Vector3D& globalVertex : globalVertices) {
		double x_local, y_local;
		// Use the planeCoordinates function to convert global to local coordinates
		if (IBKMK::planeCoordinates(offset, localX, localY, globalVertex, x_local, y_local)) {
			localVertices.emplace_back(x_local, y_local);
		}
		else {
			// Handle the case where the projection fails
			// For example, you can log an error or throw an exception
			throw std::runtime_error("Failed to project global vertex to local coordinates.");
		}
	}

	return localVertices;
}

void DrawingOSM::createMultipolygonFromWay(Way & way, Multipolygon & multipolygon)
{
	for (int i = 0; i < way.m_nd.size() ; i++) {
		const Node * node = findNodeFromId(way.m_nd[i].ref);
		Q_ASSERT(node);
		multipolygon.m_outerPolyline.push_back(convertLatLonToVector2D(node->m_lat, node->m_lon));
	}
}

void DrawingOSM::createMultipolygonsFromRelationOld(Relation & relation, std::vector<Multipolygon> & multipolygons)
{

	/* simple struct to group together the outer polyline with multiple inner polylines to form a single Polygon with possible multiple inner holes
	 * only contains references to nodes */
	struct OuterInners {
		std::vector<int> outer;
		std::vector<std::vector<int>> inners;
	};

	std::vector<OuterInners> vectorOuterInner;

	auto processWay = [&](std::vector<int>& nodeRefs, int wayRef) -> bool {
		const Way* way = findWayFromId(wayRef);
		if(!way) return false;
		for (int i = 0; i < way->m_nd.size(); i++) {
			if (!nodeRefs.empty() && nodeRefs.back() == way->m_nd[i].ref) {
				continue;
			}
			nodeRefs.push_back(way->m_nd[i].ref);
		}
		if(nodeRefs[0] == way->m_nd.back().ref && nodeRefs.size() > 1)
			return true;
		else
			return false;
	};


	/* https://wiki.openstreetmap.org/wiki/Relation:multipolygon
	 * when multiple ways appear right after another, they can form a single polygon.
	 * If the first node of the first way of a polygon is equal to the last node of a way, then that polygon is fully described.
	 * all "inner" ways that appear right after an "outer" polygon/ways create holes in that outer polygon.
	 * multiple "inner" ways can also form a single polygon that cuts a hole in the "outer" polygon */

	bool outerActive = false;
	bool innerActive = false;
	OuterInners outerInners;

	for (int i = 0; i < relation.m_members.size(); i++) {
		if (relation.m_members[i].type != WayType) continue;

		bool roleOuter = relation.m_members[i].role == "outer";
		bool roleInner = relation.m_members[i].role == "inner";

		// next member is part of an outer polygon
		if (roleOuter) {
			innerActive = false;

			// if latest outer Polygon was finished, save it as complete multipolygon
			if (!outerActive) {

				if(outerInners.outer.size() > 1) {

					for (int j = 0; j < outerInners.inners.size(); ) {
						if (outerInners.inners[j].size() == 0) {
							outerInners.inners.erase(outerInners.inners.begin() + j);
						} else {
							++j;
						}
					}

					vectorOuterInner.push_back(outerInners);
				}
				outerInners = OuterInners();
				outerActive = true;
			}

			outerActive = !processWay(outerInners.outer, relation.m_members[i].ref);
			innerActive = !outerActive;
			if (innerActive)
				outerInners.inners.push_back(std::vector<int>());

		} else if (roleInner ) {
			outerActive = false;
			if (!innerActive){
				outerInners.inners.push_back(std::vector<int>());
			}
			innerActive = !processWay(outerInners.inners.back(), relation.m_members[i].ref);
			if (!innerActive) {
				outerInners.inners.push_back(std::vector<int>());
			}
		}
	}
	if (outerInners.outer.size() > 1) {
		for (int j = 0; j < outerInners.inners.size(); ) {
			if (outerInners.inners[j].size() == 0) {
				outerInners.inners.erase(outerInners.inners.begin() + j);
			} else {
				++j;
			}
		}

		vectorOuterInner.push_back(outerInners);
	}

	for (auto& outerInners : vectorOuterInner) {
		Multipolygon multipolygon;

		for (int id : outerInners.outer) {
			const Node *node = findNodeFromId(id);
			Q_ASSERT(node);
			multipolygon.m_outerPolyline.push_back(convertLatLonToVector2D(node->m_lat, node->m_lon));
		}

		for (auto& inner : outerInners.inners) {
			std::vector<IBKMK::Vector2D> innerPolyline;
			for (int id : inner) {
				const Node *node = findNodeFromId(id);
				Q_ASSERT(node);
				innerPolyline.push_back(convertLatLonToVector2D(node->m_lat, node->m_lon));
			}
			if (innerPolyline.size() > 2)
				multipolygon.m_innerPolylines.push_back(innerPolyline);
		}

		std::vector<IBKMK::Vector3D> outerPolygon;

		for (int i = 1; i < multipolygon.m_outerPolyline.size(); i++) {
			IBKMK::Vector3D p = IBKMK::Vector3D(multipolygon.m_outerPolyline[i].m_x,
												multipolygon.m_outerPolyline[i].m_y,
												0);

			QVector3D vec = m_rotationMatrix.toQuaternion() * IBKVector2QVector(p);
			vec += IBKVector2QVector(m_origin);

			outerPolygon.push_back(QVector2IBKVector(vec));
		}

		VICUS::PlaneGeometry outerPlaneGeometry (outerPolygon);
		if (!outerPlaneGeometry.isValid()) continue;

		for (auto& innerPolyline : multipolygon.m_innerPolylines) {
			std::vector<IBKMK::Vector3D> innerPolygon;

			for (int i = 1; i < innerPolyline.size(); i++) {
				IBKMK::Vector3D p = IBKMK::Vector3D(innerPolyline[i].m_x,
													innerPolyline[i].m_y,
													0);

				QVector3D vec = m_rotationMatrix.toQuaternion() * IBKVector2QVector(p);
				vec += IBKVector2QVector(m_origin);

				innerPolygon.push_back(QVector2IBKVector(vec));
			}

			innerPolyline = convertHoleToLocalCoordinates(innerPolygon, outerPlaneGeometry.offset(), outerPlaneGeometry.localX(), outerPlaneGeometry.localY());

		}

		multipolygons.push_back(multipolygon);
	}
}

void DrawingOSM::createMultipolygonsFromRelation(Relation & relation, std::vector<Multipolygon> & multipolygons)
{
	auto processWay = [&](std::vector<int>& nodeRefs, int wayRef) -> bool {
		const Way* way = findWayFromId(wayRef);
		if(!way) return false;
		for (int i = 0; i < way->m_nd.size(); i++) {
			nodeRefs.push_back(way->m_nd[i].ref);
		}
	};

	std::vector<WayWithMarks> ways;

	for (int i = 0; i < relation.m_members.size(); i++) {
		if (relation.m_members[i].type != WayType) continue;
		WayWithMarks way;
		processWay(way.refs, relation.m_members[i].ref);
		ways.push_back(way);
	}

	std::vector<std::vector<WayWithMarks*>> rings;
	// first creates all possible areas/rings out of the set of open and closed ways
	ringAssignment(ways, rings);
	// then calculates if the areas contain each other and tries to create Multipolygons with holes
	ringGrouping(rings, multipolygons);

}

void DrawingOSM::ringAssignment(std::vector<WayWithMarks> & ways, std::vector<std::vector<WayWithMarks*>>& allRings)
{
	std::vector<WayWithMarks*> currentRing;

	// get next unassigned way for a new ring
	std::function<bool()> createNewRing = [&]() -> bool {
		currentRing.clear();
		for (auto& way : ways) {
			if (!way.assigned) {
				way.assigned = true;
				if(way.refs.empty()) {
					return createNewRing();
				}
				currentRing.push_back(&way);
				return true;
			}
		}
		return false;
	};

	// uses Polygon3D to check if the
	auto checkIfValidGeometry = [&]() -> bool {
		std::vector<IBKMK::Vector2D> polyline = convertVectorWayWithMarksToVector2D(currentRing);
		Polygon2D polygon(polyline);
		return polygon.isValid();
	};

	auto getNewWay = [&]() -> bool {
		int nodeID = currentRing.back()->reversedOrder ? currentRing.back()->refs.front() : currentRing.back()->refs.back();
		for (auto& way : ways) {
			if (way.assigned || way.selected) continue;
			if (way.refs.empty()) {
				way.assigned = true;
				continue;
			}
			if (way.refs.back() == nodeID) {
				way.reversedOrder = true;
				way.selected = true;
				currentRing.push_back(&way);
				return true;
			} else if (way.refs.front() == nodeID) {
				way.selected = true;
				currentRing.push_back(&way);
				return true;
			}
		}
		return false;
	};

	auto constructRing = [&]() -> bool {
		while(true) {
			if (!currentRing.front()->refs.empty() && !currentRing.back()->refs.empty() &&
				currentRing.front()->refs[0] == (currentRing.back()->reversedOrder ? currentRing.back()->refs.front() : currentRing.back()->refs.back())) {
				if (checkIfValidGeometry()) {
					for (WayWithMarks* way : currentRing) {
						way->assigned = true;
					}
					allRings.push_back(currentRing);
					return true;
				} else {
					return false;
				}
			} else {
				if (getNewWay()) {
					continue;
				} else {
					for (auto& way : currentRing) {
						way->selected = false;
					}
				}
			}
		}
	};

	while (createNewRing()) {
		constructRing();
	}
}

std::vector<IBKMK::Vector2D> DrawingOSM::convertVectorWayWithMarksToVector2D(const std::vector<WayWithMarks*>& ways){
	std::vector<IBKMK::Vector2D> vectorCoordinates;
	for (WayWithMarks * way : ways) {
		if (way->reversedOrder) {
			for (int i = way->refs.size() - 1; i >= 0; i--) {
				const Node *node = findNodeFromId(way->refs[i]);
				Q_ASSERT(node);
				vectorCoordinates.push_back(convertLatLonToVector2D(node->m_lat, node->m_lon));
			}
		} else {
			for (int refs : way->refs) {
				const Node *node = findNodeFromId(refs);
				Q_ASSERT(node);
				vectorCoordinates.push_back(convertLatLonToVector2D(node->m_lat, node->m_lon));
			}
		}
	}
	return vectorCoordinates;
}

void DrawingOSM::ringGrouping(std::vector<std::vector<WayWithMarks *> >& rings, std::vector<Multipolygon>& multipolygons)
{

	// create Matrix and reset available flags
	// matrix[i][j] true if ring i is in ring j
	std::vector<std::vector<bool>> matrix;
	std::vector<std::vector<IBKMK::Vector2D>> ringsVector;

	for (auto& ring : rings) {
		ringsVector.push_back(convertVectorWayWithMarksToVector2D(ring));
	}

	std::vector<int> usedRings;
	int size = rings.size();
	for (int i = 0; i < size; i++) {
		std::vector<bool> row;
		for(WayWithMarks * way : rings[i]) way->assigned = false;
		row.resize(size);
		for (int j = 0; j < size; j++) {
			// ring does not contain itself
			if(i == j) {
				row[j] = false;
			}
			int contained = 1 == IBKMK::polygonInPolygon(ringsVector[i], ringsVector[j]);
			row[j] = contained;
		}
		matrix.push_back(row);
	}

	int activeOuterRing;
	std::function<bool()> findOuterRing = [&]() -> bool {
		for (unsigned int i = 0; i < rings.size(); i++) {
			if (std::find(usedRings.begin(), usedRings.end(), i) != usedRings.end())  continue;
			bool isInnerRing = false;
			for (unsigned int j = 0; j < matrix.size(); j++) {
				if (std::find(usedRings.begin(), usedRings.end(), j) != usedRings.end())  continue;
				if (matrix[i][j]) {
					isInnerRing = true;
					break;
				}
			}

			if (!isInnerRing) {
				activeOuterRing = i;
				usedRings.push_back(i);
				return true;
			}
		}
		return false;
	};

	// checks if ring is contained in the active Outer Ring and not contained in any other unused ring
	std::function<void(Multipolygon&)> addAllInners = [&](Multipolygon& multipolygon) {
		for (unsigned int i = 0; i < matrix.size(); i++) {
			if (std::find(usedRings.begin(), usedRings.end(), i) != usedRings.end())  continue;
			if (matrix[i][activeOuterRing]) {
				for (unsigned int j = 0; j < matrix.size(); j++) {
					if (std::find(usedRings.begin(), usedRings.end(), j) != usedRings.end())  continue;
					if (j != activeOuterRing && matrix[i][j]) {
						continue;
					}
				}
				usedRings.push_back(i);
				multipolygon.m_innerPolylines.push_back(ringsVector[i]);
			}
		}


		/* converts coordinates of holes to coordinate system of outer polygon to make it compatible
		 * with PlaneGeometry setHoles */

		std::vector<IBKMK::Vector3D> outerPolygon;

		for (int i = 1; i < multipolygon.m_outerPolyline.size(); i++) {
			IBKMK::Vector3D p = IBKMK::Vector3D(multipolygon.m_outerPolyline[i].m_x,
												multipolygon.m_outerPolyline[i].m_y,
												0);

			QVector3D vec = m_rotationMatrix.toQuaternion() * IBKVector2QVector(p);
			vec += IBKVector2QVector(m_origin);

			outerPolygon.push_back(QVector2IBKVector(vec));
		}

		VICUS::PlaneGeometry outerPlaneGeometry (outerPolygon);
		if (!outerPlaneGeometry.isValid()) return;

		for (auto& innerPolyline : multipolygon.m_innerPolylines) {
			std::vector<IBKMK::Vector3D> innerPolygon;

			for (int i = 1; i < innerPolyline.size(); i++) {
				IBKMK::Vector3D p = IBKMK::Vector3D(innerPolyline[i].m_x,
													innerPolyline[i].m_y,
													0);

				QVector3D vec = m_rotationMatrix.toQuaternion() * IBKVector2QVector(p);
				vec += IBKVector2QVector(m_origin);

				innerPolygon.push_back(QVector2IBKVector(vec));
			}

			innerPolyline = convertHoleToLocalCoordinates(innerPolygon, outerPlaneGeometry.offset(), outerPlaneGeometry.localX(), outerPlaneGeometry.localY());

		}

		multipolygons.push_back(multipolygon);

	};

	while(findOuterRing()) {
		Multipolygon multipolygon;
		multipolygon.m_outerPolyline = ringsVector[activeOuterRing];
		addAllInners(multipolygon);
	}

}

void DrawingOSM::readXML(const TiXmlElement * element) {
	FUNCID(DrawingOSM::readXML);

	try {
		// search for mandatory attributes

		// reading attributes
		const TiXmlAttribute * attrib = element->FirstAttribute();
		while (attrib) {
			const std::string & attribName = attrib->NameStr();
			if (attribName == "bounds") {
			}
			attrib = attrib->Next();
		}
		// search for mandatory elements
		// reading elements
		const TiXmlElement * c = element->FirstChildElement();
		while (c) {
			const std::string & cName = c->ValueStr();
			if (cName == "bounds") {
				// search for mandatory attributes
				if (!TiXmlAttribute::attributeByName(c, "minlat"))
					throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(c->Row()).arg(
											 IBK::FormatString("Missing required 'minlat' attribute.") ), FUNC_ID);
				if (!TiXmlAttribute::attributeByName(c, "minlon"))
					throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(c->Row()).arg(
											 IBK::FormatString("Missing required 'minlon' attribute.") ), FUNC_ID);
				if (!TiXmlAttribute::attributeByName(c, "maxlat"))
					throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(c->Row()).arg(
											 IBK::FormatString("Missing required 'maxlat' attribute.") ), FUNC_ID);
				if (!TiXmlAttribute::attributeByName(c, "maxlon"))
					throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(c->Row()).arg(
											 IBK::FormatString("Missing required 'maxlon' attribute.") ), FUNC_ID);

				// reading attributes
				const TiXmlAttribute * attrib = c->FirstAttribute();
				while (attrib) {
					const std::string & attribName = attrib->NameStr();
					if (attribName == "minlat")
						m_boundingBox.minlat = attrib->DoubleValue();
					else if (attribName == "minlon")
						m_boundingBox.minlon = attrib->DoubleValue();
					else if (attribName == "maxlat")
						m_boundingBox.maxlat = attrib->DoubleValue();
					else if (attribName == "maxlon")
						m_boundingBox.maxlon = attrib->DoubleValue();
					attrib = attrib->Next();
				}
			}
			else if (cName == "node") {
				Node obj;
				obj.readXML(c);
				m_nodes[obj.m_id] = obj;
			}
			else if (cName == "way") {
				Way obj ;
				obj.readXML(c);
				m_ways[obj.m_id] = obj;
			}
			else if (cName == "relation") {
				Relation obj;
				obj.readXML(c);
				m_relations[obj.m_id] = obj;
			}
			c = c->NextSiblingElement();
		}
	}
	catch (IBK::Exception & ex) {
		throw IBK::Exception( ex, IBK::FormatString("Error reading 'Drawing' element."), FUNC_ID);
	}
	catch (std::exception & ex2) {
		throw IBK::Exception( IBK::FormatString("%1\nError reading 'Drawing::Text' element.").arg(ex2.what()), FUNC_ID);
	}
}

bool DrawingOSM::readOSMFile(QString filePath)
{
	TiXmlDocument document(filePath.toStdString());

	if (!document.LoadFile()) {
		qDebug() << "Failed to load file: " << filePath;
		qDebug() << "Error: " << document.ErrorDesc();
		return false;
	}

	TiXmlElement* root = document.RootElement();
	if (!root) {
		qDebug() << "Failed to get root element";
		return false;
	}

	// Print the root element's name
	qDebug() << "Root element: " << root->Value();

	readXML(document.RootElement());
	double minx, miny, maxx, maxy;
	m_utmZone = IBKMK::LatLonToUTMXY(m_boundingBox.minlat, m_boundingBox.minlon, m_utmZone, minx, miny);
	IBKMK::LatLonToUTMXY(m_boundingBox.maxlat, m_boundingBox.maxlon, m_utmZone, maxx, maxy);

	m_centerMercatorProjection = IBKMK::Vector2D(minx + (maxx - minx) / 2,miny + (maxy - miny) / 2);
	qDebug() << "m_origin: " << QString::number(minx + (maxx - minx) / 2) << " " << QString::number(miny + (maxy - miny) / 2);
	qDebug() << "m_utmZone: " << QString::number(m_utmZone);

	m_filePath = filePath;

	return true;
}

void DrawingOSM::constructObjects()
{
	for (auto& pair : m_ways) {
		if (pair.second.containsKey("building"))
			createBuilding(pair.second);
		else if (pair.second.containsKey("place") || pair.second.containsKey("heritage"))
			createPlace(pair.second);
		else if (pair.second.containsKey("highway"))
			createHighway(pair.second);
		else if (pair.second.containsKey("water") || pair.second.containsKey("waterway"))
			createWater(pair.second);
		else if (pair.second.containsKey("landuse"))
			createLand(pair.second);
		else if (pair.second.containsKey("leisure"))
			createLeisure(pair.second);
		else if (pair.second.containsKey("natural"))
			createNatural(pair.second);
		else if (pair.second.containsKey("amenity"))
			createAmenity(pair.second);
	}

	for (auto& pair : m_relations) {
		if (pair.second.containsKey("building"))
			createBuilding(pair.second);
		else if (pair.second.containsKey("place"))
			createPlace(pair.second);
		else if (pair.second.containsKey("water") || pair.second.containsKey("waterway"))
			createWater(pair.second);
		else if (pair.second.containsKey("highway"))
			createHighway(pair.second);
		else if (pair.second.containsKey("landuse"))
			createLand(pair.second);
	}
}

void DrawingOSM::updatePlaneGeometries()
{
	for (auto& building : m_buildings) {
		for (auto& areaBorder : building.m_areaBorders ){
			areaBorder.updatePlaneGeometry();
		}
	}
}

const void DrawingOSM::geometryData(std::map<double, std::vector<GeometryData *>>& geometryData) const
{
	for (const auto & building : m_buildings) {
		building.addGeometryData(geometryData[building.m_zPosition]);

		// 3D stuff
		// for(auto& areaBorder : building.m_areaBorders){
		// 	if (areaBorder.m_multipolygon) continue;
		// 	addPolygonExtrusion(areaBorder.m_polyline, building.m_height, areaBorder.m_colorArea, currentVertexIndex, currentElementIndex,
		// 						m_drawingOSMGeometryObject.m_vertexBufferData,
		// 						m_drawingOSMGeometryObject.m_colorBufferData,
		// 						m_drawingOSMGeometryObject.m_indexBufferData);
		// }
	}

	for( const auto & highway : m_highways) {
		highway.addGeometryData(geometryData[highway.m_zPosition]);
	}

	for( const auto & water : m_waters) {
		water.addGeometryData(geometryData[water.m_zPosition]);
	}

	for( const auto & land : m_land) {
		land.addGeometryData(geometryData[land.m_zPosition]);
	}

	for( const auto & leisure : m_leisure) {
		leisure.addGeometryData(geometryData[leisure.m_zPosition]);
	}

	for( const auto & natural : m_natural) {
		natural.addGeometryData(geometryData[natural.m_zPosition]);
	}

	for( const auto & amenity : m_amenities) {
		amenity.addGeometryData(geometryData[amenity.m_zPosition]);
	}

	for( const auto & place : m_places) {
		place.addGeometryData(geometryData[place.m_zPosition]);
	}
}

const DrawingOSM::Node * DrawingOSM::findNodeFromId(unsigned int id) const
{
	auto it = m_nodes.find(id);
	if (it != m_nodes.end()) {
		return &it->second;
	} else {
		return nullptr;
	}
}

const DrawingOSM::Way * DrawingOSM::findWayFromId(unsigned int id) const
{
	auto it = m_ways.find(id);
	if (it != m_ways.end()) {
		return &it->second;
	} else {
		return nullptr;
	}
}

const DrawingOSM::Relation * DrawingOSM::findRelationFromId(unsigned int id) const
{
	auto it = m_relations.find(id);
	if (it != m_relations.end()) {
		return &it->second;
	} else {
		return nullptr;
	}
}

inline IBKMK::Vector2D DrawingOSM::convertLatLonToVector2D(double lat, double lon)
{
	double x, y;
	IBKMK::LatLonToUTMXY(lat, lon, m_utmZone, x, y);
	return IBKMK::Vector2D(x, y) - m_centerMercatorProjection;
}

void DrawingOSM::createBuilding(Way & way) {
	if (way.containsKeyValue("building", "cellar")) return;
	if (way.containsKeyValue("building", "roof")) return;
	Building building;
	building.initialize(way);
	building.m_zPosition = 5;

	AreaBorder areaBorder(this);
	//if(way.containsKeyValue("layer", "-1")) areaBorder.m_zPosition = 0;

	createMultipolygonFromWay(way, areaBorder.m_multiPolygon);

	building.m_areaBorders.push_back(areaBorder);
	m_buildings.push_back(building);
}

void DrawingOSM::createBuilding(Relation & relation) {
	if (!relation.containsKeyValue("type", "multipolygon")) return;
	if (relation.containsKeyValue("building", "cellar")) return;
	if (relation.containsKeyValue("building", "roof")) return;
	Building building;
	building.initialize(relation);
	building.m_zPosition = 5;

	std::vector<Multipolygon> multipolygons;
	createMultipolygonsFromRelation(relation, multipolygons);

	for (auto multipolygon : multipolygons) {
		AreaBorder areaBorder(this);
		areaBorder.m_multiPolygon = multipolygon;
		building.m_areaBorders.push_back(areaBorder);
	}

	m_buildings.push_back(building);
}

void DrawingOSM::createHighway(Way & way)
{
	Highway highway;
	highway.initialize(way);
	highway.m_zPosition = 3;
	bool area = way.containsKeyValue("area", "yes");

	if (area) {
		AreaBorder areaBorder(this);
		//if(way.containsKeyValue("layer", "-1")) areaBorder.m_zPosition = 0;
		areaBorder.m_colorArea = QColor("#78909c");

		createMultipolygonFromWay(way, areaBorder.m_multiPolygon);

		highway.m_areaBorders.push_back(areaBorder);
	} else  {
		LineFromPlanes lineFromPlanes(this);
		lineFromPlanes.m_lineThickness = 3.5;
		lineFromPlanes.m_color = QColor("#78909c");
		for (int i = 0; i < way.m_nd.size() ; i++) {
			const Node * node = findNodeFromId(way.m_nd[i].ref);
			Q_ASSERT(node);
			lineFromPlanes.m_polyline.push_back(convertLatLonToVector2D(node->m_lat, node->m_lon));
		}
		highway.m_linesFromPlanes.push_back(lineFromPlanes);
	}

	m_highways.push_back(highway);
}

void DrawingOSM::createHighway(Relation & relation)
{
	bool containsKey = relation.containsKeyValue("highway", "pedestrian") || relation.containsKeyValue("highway", "footway");
	bool isMultipolygon = relation.containsKeyValue("type", "multipolygon");
	if (!(containsKey && isMultipolygon)) return;

	Highway highway;
	highway.initialize(relation);
	highway.m_zPosition = 0;

	std::vector<Multipolygon> multipolygons;
	createMultipolygonsFromRelation(relation, multipolygons);

	for (auto multipolygon : multipolygons) {
		AreaBorder areaBorder(this);
		areaBorder.m_multiPolygon = multipolygon;
		areaBorder.m_colorArea = QColor("#78909c");
		highway.m_areaBorders.push_back(areaBorder);
	}

	m_highways.push_back(highway);
}

void DrawingOSM::createWater(Way & way)
{
	Water water;
	water.initialize(way);
	bool containsWater = way.containsKey("water");

	if (containsWater) /* water */ {
		AreaNoBorder areaNoBorder(this);
		areaNoBorder.m_color = QColor("#aad3df");
		water.m_zPosition = 2;
		createMultipolygonFromWay(way, areaNoBorder.m_multiPolygon);
		water.m_areaNoBorders.push_back(areaNoBorder);

	} else /* waterway */ {
		LineFromPlanes lineFromPlanes(this);
		lineFromPlanes.m_lineThickness = 1;
		water.m_zPosition = 1.95;
		for (int i = 0; i < way.m_nd.size() ; i++) {
			const Node * node = findNodeFromId(way.m_nd[i].ref);
			Q_ASSERT(node);
			lineFromPlanes.m_polyline.push_back(convertLatLonToVector2D(node->m_lat, node->m_lon));
		}
		water.m_linesFromPlanes.push_back(lineFromPlanes);
	}
	m_waters.push_back(water);
}

void DrawingOSM::createWater(Relation & relation)
{
	if(!(relation.containsKeyValue("type", "multipolygon"))) return;
	Water water;
	water.initialize(relation);
	water.m_zPosition = 2;

	std::vector<Multipolygon> multipolygons;
	createMultipolygonsFromRelation(relation, multipolygons);

	for (auto multipolygon : multipolygons) {
		AreaNoBorder areaNoBorder(this);
		areaNoBorder.m_multiPolygon = multipolygon;
		areaNoBorder.m_color = QColor("#aad3df");
		water.m_areaNoBorders.push_back(areaNoBorder);
	}

	m_waters.push_back(water);
}

void DrawingOSM::createLand(Way & way)
{
	Land land;
	std::string value = way.getValueFromKey("landuse");

	AreaNoBorder areaNoBorder(this);
	areaNoBorder.m_color = QColor("#c8facc");
	land.m_zPosition = 1;

	createMultipolygonFromWay(way, areaNoBorder.m_multiPolygon);

	if (value == "residential"){
		areaNoBorder.m_color = QColor("#f2dad9");
		land.m_zPosition = 0.6;
	} else if (value == "forest") {
		areaNoBorder.m_color = QColor("#add19e");
		land.m_zPosition = 0.25;
	} else if (value == "industrial") {
		areaNoBorder.m_color = QColor("#ebdbe8");
		land.m_zPosition = 0.4;
	} else if (value == "village_green") {
		areaNoBorder.m_color = QColor("#cdebb0");
		land.m_zPosition = 0.1;
	} else if (value == "construction") {
		areaNoBorder.m_color = QColor("#c7c7b4");
		land.m_zPosition = 0.45;
	} else if (value == "grass") {
		areaNoBorder.m_color = QColor("#cdebb0");
		land.m_zPosition = 0.15;
	} else if (value == "retail") {
		areaNoBorder.m_color = QColor("#ffd6d1");
		land.m_zPosition = 0.5;
	} else if (value == "cemetery") {
		areaNoBorder.m_color = QColor("#aacbaf");
		land.m_zPosition = 0.85;
	} else if (value == "commercial") {
		areaNoBorder.m_color = QColor("#f2dad9");
		land.m_zPosition = 0.55;
	} else if (value == "public_administration") {
		areaNoBorder.m_color = QColor("#f2efe9");
		land.m_zPosition = 0.7;
	} else if (value == "railway") {
		areaNoBorder.m_color = QColor("#ebdbe8");
		land.m_zPosition = 0.65;
	} else if (value == "farmyard") {
		areaNoBorder.m_color = QColor("#f5dcba");
		land.m_zPosition = 0.3;
	} else if (value == "meadow") {
		areaNoBorder.m_color = QColor("#cdebb0");
		land.m_zPosition = 1.8;
	} else if (value == "religious") {
		areaNoBorder.m_color = QColor("#d0d0d0");
		land.m_zPosition = 0.75;
	} else if (value == "flowerbed") {
		areaNoBorder.m_color = QColor("#cdebb0");
		land.m_zPosition = 1.9;
	} else if (value == "recreation_ground") {
		areaNoBorder.m_color = QColor("#dffce2");
		land.m_zPosition = 0.8;
	} else if (value == "brownfield") {
		areaNoBorder.m_color = QColor("#c7c7b4");
		land.m_zPosition = 0.2;
	}
	land.m_areaNoBorders.push_back(areaNoBorder);
	m_land.push_back(land);
}

void DrawingOSM::createLand(Relation& relation){
	if(!(relation.containsKeyValue("type", "multipolygon"))) return;
	Land land;
	land.initialize(relation);
	std::string value = relation.getValueFromKey("landuse");

	AreaNoBorder areaNoBorder(this);
	QColor color = QColor("#c8facc");
	double zPosition = 1;

	std::vector<Multipolygon> multipolygons;
	createMultipolygonsFromRelation(relation, multipolygons);

	if (value == "residential"){
		color = QColor("#f2dad9");
		zPosition = 0.6;
	} else if (value == "forest") {
		color = QColor("#add19e");
		zPosition = 0.25;
	} else if (value == "industrial") {
		color = QColor("#ebdbe8");
		zPosition = 0.4;
	} else if (value == "village_green") {
		color = QColor("#cdebb0");
		zPosition = 0.1;
	} else if (value == "construction") {
		color = QColor("#c7c7b4");
		zPosition = 0.45;
	} else if (value == "grass") {
		color = QColor("#cdebb0");
		zPosition = 0.15;
	} else if (value == "retail") {
		color = QColor("#ffd6d1");
		zPosition = 0.5;
	} else if (value == "cemetery") {
		color = QColor("#aacbaf");
		zPosition = 0.85;
	} else if (value == "commercial") {
		color = QColor("#f2dad9");
		zPosition = 0.55;
	} else if (value == "public_administration") {
		color = QColor("#f2efe9");
		zPosition = 0.7;
	} else if (value == "railway") {
		color = QColor("#ebdbe8");
		zPosition = 0.65;
	} else if (value == "farmyard") {
		color = QColor("#f5dcba");
		zPosition = 0.3;
	} else if (value == "meadow") {
		color = QColor("#cdebb0");
		zPosition = 1.8;
	} else if (value == "religious") {
		color = QColor("#d0d0d0");
		zPosition = 0.75;
	} else if (value == "flowerbed") {
		color = QColor("#cdebb0");
		zPosition = 1.9;
	} else if (value == "recreation_ground") {
		color = QColor("#dffce2");
		zPosition = 0.8;
	} else if (value == "brownfield") {
		color = QColor("#c7c7b4");
		zPosition = 0.2;
	}

	land.m_zPosition = zPosition;

	for (auto multipolygon : multipolygons) {
		AreaNoBorder areaNoBorder(this);
		areaNoBorder.m_color = color;
		areaNoBorder.m_multiPolygon = multipolygon;
		land.m_areaNoBorders.push_back(areaNoBorder);
	}

	m_land.push_back(land);
}

void DrawingOSM::createLeisure(Way & way)
{
	Leisure leisure;
	leisure.initialize(way);
	leisure.m_zPosition = 1;
	std::string value = way.getValueFromKey("leisure");

	AreaNoBorder areaNoBorder(this);
	areaNoBorder.m_color = QColor("#c8facc");

	createMultipolygonFromWay(way, areaNoBorder.m_multiPolygon);

	if (value == "park"){
		areaNoBorder.m_color = QColor("#c8facc");
	}

	leisure.m_areaNoBorders.push_back(areaNoBorder);
	m_leisure.push_back(leisure);
}

void DrawingOSM::createNatural(Way & way)
{
	Natural natural;
	natural.initialize(way);
	std::string value = way.getValueFromKey("natural");

	bool noArea = false;

	QColor color = QColor("#c8facc");
	double zPosition = 1.5;

	if (value == "water"){
		color = QColor("#aad3df");
		zPosition = 2;
	} else if (value == "tree_row") {
		color = QColor("#b8d4a7");
		zPosition = 1.575;
		noArea = true;
	}

	natural.m_zPosition = zPosition;

	if (noArea) {
		LineFromPlanes lineFromPlanes(this);
		lineFromPlanes.m_color = color;
		lineFromPlanes.m_lineThickness = 3;

		//if(way.containsKeyValue("layer", "-1")) lineFromPlanes.m_zPosition = 0;
		for (int i = 0; i < way.m_nd.size() ; i++) {
			const Node * node = findNodeFromId(way.m_nd[i].ref);
			Q_ASSERT(node);
			lineFromPlanes.m_polyline.push_back(convertLatLonToVector2D(node->m_lat, node->m_lon));
		}

		natural.m_linesFromPlanes.push_back(lineFromPlanes);
		m_natural.push_back(natural);
		return;
	}

	AreaNoBorder areaNoBorder(this);
	areaNoBorder.m_color = color;

	createMultipolygonFromWay(way, areaNoBorder.m_multiPolygon);

	natural.m_areaNoBorders.push_back(areaNoBorder);
	m_natural.push_back(natural);
}

void DrawingOSM::createAmenity(Way & way)
{
	Amenity amenity;
	amenity.initialize(way);
	amenity.m_zPosition = 1.75;
	std::string value = way.getValueFromKey("amenity");

	AreaNoBorder areaNoBorder(this);
	areaNoBorder.m_color = QColor("#c8facc");

	createMultipolygonFromWay(way, areaNoBorder.m_multiPolygon);

	if (value == "parking") {
		areaNoBorder.m_color = QColor("#eeeeee");
	} else if (value == "kindergarten") {
		areaNoBorder.m_color = QColor("#ffffe5");
	} else if (value == "school") {
		areaNoBorder.m_color = QColor("#ffffe5");
	} else if (value == "social_facility") {
		areaNoBorder.m_color = QColor("#ffffe5");
	}

	amenity.m_areaNoBorders.push_back(areaNoBorder);
	m_amenities.push_back(amenity);
}

void DrawingOSM::createPlace(Way & way)
{
	Place place;
	place.initialize(way);
	place.m_zPosition = 1.75;
	std::string value = way.getValueFromKey("amenity");

	AreaBorder areaBorder(this);
	areaBorder.m_colorArea = QColor("#dddde8");

	createMultipolygonFromWay(way, areaBorder.m_multiPolygon);

	place.m_areaBorders.push_back(areaBorder);
	m_places.push_back(place);
}

void DrawingOSM::createPlace(Relation & relation)
{
	if (!relation.containsKeyValue("type", "multipolygon")) return;
	Place place;
	place.initialize(relation);
	place.m_zPosition = 2;

	std::vector<Multipolygon> multipolygons;
	createMultipolygonsFromRelation(relation, multipolygons);

	for (auto multipolygon : multipolygons) {
		AreaBorder areaBorder(this);
		areaBorder.m_multiPolygon = multipolygon;
		areaBorder.m_colorArea = QColor("#dddde8");
		place.m_areaBorders.push_back(areaBorder);
	}

	m_places.push_back(place);
}

bool DrawingOSM::generatePlanesFromPolyline(const std::vector<IBKMK::Vector3D> & polyline, bool connectEndStart, double width, std::vector<PlaneGeometry> & planes) const
{
	// initialise values
	IBKMK::Vector3D lineVector, previousVector, crossProduct, perpendicularVector;
	std::vector<IBKMK::Vector3D> previousVertices;
	double halfWidth = width / 2;
	double length;

	// if polyline is empty, return
	if(polyline.size() < 2){
		return false;
	}

	// initialise previousVector
	previousVector = polyline[1] - polyline[0];

	auto processSegment = [&](const IBKMK::Vector3D& startPoint, const IBKMK::Vector3D& endPoint)->void {
		// calculate line vector
		lineVector = endPoint - startPoint;
		length = lineVector.magnitude();
		if(length <= 0)
			return;

		IBKMK::Vector3D normal(m_rotationMatrix.toQuaternion().toRotationMatrix()(0,2),
							   m_rotationMatrix.toQuaternion().toRotationMatrix()(1,2),
							   m_rotationMatrix.toQuaternion().toRotationMatrix()(2,2));

		// calculate perpendicular vector
		perpendicularVector = lineVector.crossProduct(normal);
		perpendicularVector.normalize();
		perpendicularVector *= halfWidth;

		// create vertices for the line
		std::vector<IBKMK::Vector3D> lineVertices = {
			startPoint - perpendicularVector,
			endPoint - perpendicularVector,
			endPoint + perpendicularVector,
			startPoint + perpendicularVector,
		};

		// Transformation for block segment
		// Draw the line
		IBKMK::Polygon3D p(VICUS::Polygon2D::T_Rectangle, lineVertices[0], lineVertices[3], lineVertices[1]);
		planes.push_back(VICUS::PlaneGeometry(p));

		// Calculate the cross product between the current line Vector and previous to get the direction of the triangle
		crossProduct = lineVector.crossProduct(previousVector);

		if (previousVertices.size() == lineVertices.size()) {
			// draws the triangle
			if(crossProduct.m_z > 1e-10){
				// line is left
				std::vector<IBKMK::Vector3D> verts(3);
				verts[0] = previousVertices[1];
				verts[1] = startPoint;
				verts[2] = lineVertices[0];

				IBKMK::Polygon3D poly3d(verts);
				planes.push_back(PlaneGeometry(poly3d));
			}
			else if(crossProduct.m_z < -1e-10){
				// line is right

				// line is left
				std::vector<IBKMK::Vector3D> verts(3);
				verts[0] = lineVertices[3];
				verts[1] = previousVertices[2];
				verts[2] = startPoint;

				IBKMK::Polygon3D poly3d(verts);
				planes.push_back(PlaneGeometry(poly3d));
			}
			else {
				// if z coordinate of cross product is 0, lines are parallel, no triangle needed (would crash anyway)
				previousVector = lineVector;
				previousVertices = lineVertices;
				return;
			}
		}

		// update previous values
		previousVector = lineVector;
		previousVertices = lineVertices;
	};

	// loops through all points in polyline, draws a line between every two points, adds a triangle between two lines to fill out the gaps
	for (unsigned int i = 0; i < polyline.size() - 1; i++) {
		processSegment(polyline[i], polyline[i+1]);
	}

	// repeats the code of the for loop for the last line and adds two triangles to fill out the lines
	if(connectEndStart){
		unsigned int lastIndex = polyline.size() - 1;
		processSegment(polyline[lastIndex], polyline[0]);
		processSegment(polyline[0], polyline[1]);
	}

	return true;
}

void DrawingOSM::AbstractOSMElement::readXML(const TiXmlElement * element){
	FUNCID(DrawingOSM::AbstractOSMElement::readXML);

	// mandatory attributes
	if (!TiXmlAttribute::attributeByName(element, "id"))
		throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'id' attribute.") ), FUNC_ID);

	// Read common attributes
	const TiXmlAttribute * attrib = element->FirstAttribute();
	while (attrib) {
		const std::string & attribName = attrib->NameStr();
		if (attribName == "id") {
			m_id = attrib->IntValue();
		}
		else if (attribName == "visible") {
			std::string value = attrib->ValueStr();
			m_visible = value == "true" ? true : false;
		}
		attrib = attrib->Next();
	}
}

bool DrawingOSM::AbstractOSMElement::containsKey(const std::string& key) const
{
	for (int i = 0; i < m_tags.size(); i++) {
		if(m_tags[i].key == key)
			return true;
	}
	return false;
}


bool DrawingOSM::AbstractOSMElement::containsValue(const std::string& value) const
{
	for (int i = 0; i < m_tags.size(); i++) {
		if(m_tags[i].value == value)
			return true;
	}
	return false;
}

bool DrawingOSM::AbstractOSMElement::containsKeyValue(const std::string & key, const std::string & value) const
{
	for (int i = 0; i < m_tags.size(); i++) {
		if(m_tags[i].key == key && m_tags[i].value == value)
			return true;
	}
	return false;
}

std::string DrawingOSM::AbstractOSMElement::getValueFromKey(const std::string & key) const
{
	for (int i = 0; i < m_tags.size(); i++) {
		if(m_tags[i].key == key)
			return m_tags[i].value;
	}
	return std::string("");
}

void DrawingOSM::Nd::readXML(const TiXmlElement * element) {
	FUNCID(DrawingOSM::nd::readXML);

	if (!TiXmlAttribute::attributeByName(element, "ref"))
		throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'ref' attribute.") ), FUNC_ID);

	ref = TiXmlAttribute::attributeByName(element, "ref")->IntValue();
}

void DrawingOSM::Member::readXML(const TiXmlElement * element) {
	FUNCID(DrawingOSM::Member::readXML);

	if (!TiXmlAttribute::attributeByName(element, "type"))
		throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'type' attribute.") ), FUNC_ID);
	if (!TiXmlAttribute::attributeByName(element, "ref"))
		throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'ref' attribute.") ), FUNC_ID);
	if (!TiXmlAttribute::attributeByName(element, "role"))
		throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'role' attribute.") ), FUNC_ID);

	std::string typeStr = TiXmlAttribute::attributeByName(element, "type")->ValueStr();
	if (typeStr == "node")
		type = NodeType;
	else if (typeStr == "way")
		type = WayType;
	else if (typeStr == "relation")
		type = RelationType;
	else
		throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Unknown type '%1'.").arg(typeStr) ), FUNC_ID);

	ref = TiXmlAttribute::attributeByName(element, "ref")->IntValue();
	role = TiXmlAttribute::attributeByName(element, "role")->ValueStr();
}

void DrawingOSM::Tag::readXML(const TiXmlElement * element) {
	FUNCID(DrawingOSM::Tag::readXML);

	if (!TiXmlAttribute::attributeByName(element, "k"))
		throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'k' attribute.") ), FUNC_ID);
	if (!TiXmlAttribute::attributeByName(element, "v"))
		throw IBK::Exception( IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'v' attribute.") ), FUNC_ID);

	key = TiXmlAttribute::attributeByName(element, "k")->ValueStr();
	value = TiXmlAttribute::attributeByName(element, "v")->ValueStr();
}

void DrawingOSM::Node::readXML(const TiXmlElement * element) {
	FUNCID(DrawingOSM::Node::readXML);

	AbstractOSMElement::readXML(element);

	// Check for mandatory attributes
	if (!TiXmlAttribute::attributeByName(element, "lat"))
		throw IBK::Exception(IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'lat' attribute.")), FUNC_ID);
	if (!TiXmlAttribute::attributeByName(element, "lon"))
		throw IBK::Exception(IBK::FormatString(XML_READ_ERROR).arg(element->Row()).arg(
								 IBK::FormatString("Missing required 'lon' attribute.")), FUNC_ID);

	// Read the latitude and longitude attributes
	m_lat = TiXmlAttribute::attributeByName(element, "lat")->DoubleValue();
	m_lon = TiXmlAttribute::attributeByName(element, "lon")->DoubleValue();

	// Read child elements
	const TiXmlElement * child = element->FirstChildElement();
	while (child) {
		const std::string & childName = child->ValueStr();
		if (childName == "tag") {
			Tag tag;
			tag.readXML(child);
			m_tags.push_back(tag);
		}
		child = child->NextSiblingElement();
	}
}

void DrawingOSM::Way::readXML(const TiXmlElement * element) {
	FUNCID(DrawingOSM::Way::readXML);

	AbstractOSMElement::readXML(element);

	// Read child elements
	const TiXmlElement * child = element->FirstChildElement();
	while (child) {
		const std::string & childName = child->ValueStr();
		if (childName == "tag") {
			Tag tag;
			tag.readXML(child);
			m_tags.push_back(tag);
		}
		if (childName == "nd") {
			Nd nd;
			nd.readXML(child);
			m_nd.push_back(nd);
		}
		child = child->NextSiblingElement();
	}
}

void DrawingOSM::Relation::readXML(const TiXmlElement * element) {
	FUNCID(DrawingOSM::Relation::readXML);

	AbstractOSMElement::readXML(element);

	// Read child elements
	const TiXmlElement * child = element->FirstChildElement();
	while (child) {
		const std::string & childName = child->ValueStr();
		if (childName == "tag") {
			Tag tag;
			tag.readXML(child);
			m_tags.push_back(tag);
		}
		if (childName == "member") {
			Member member;
			member.readXML(child);
			m_members.push_back(member);
		}
		child = child->NextSiblingElement();
	}
}

const void DrawingOSM::AreaBorder::addGeometryData(std::vector<VICUS::DrawingOSM::GeometryData*>& data) const
{
	FUNCID(DrawingOSM::AreaBorder::addGeometryData);
	try {
		if (m_dirtyTriangulation) {
			m_geometryData.clear();

			std::vector<IBKMK::Vector3D> areaPoints;

			for (int i = 1; i < m_multiPolygon.m_outerPolyline.size(); i++) {
				IBKMK::Vector3D p = IBKMK::Vector3D(m_multiPolygon.m_outerPolyline[i].m_x,
													m_multiPolygon.m_outerPolyline[i].m_y,
													0);

				QVector3D vec = m_drawing->m_rotationMatrix.toQuaternion() * IBKVector2QVector(p);
				vec += IBKVector2QVector(m_drawing->m_origin);

				areaPoints.push_back(QVector2IBKVector(vec));
			}

			VICUS::Polygon3D polygon3D(areaPoints);
			GeometryData geometryData;
			// Initialize PlaneGeometry with the polygon
			geometryData.m_planeGeometry.push_back(VICUS::PlaneGeometry(polygon3D));

			if(!m_multiPolygon.m_innerPolylines.empty()) {
				std::vector<PlaneGeometry::Hole> holes;
				for(int j = 0; j < m_multiPolygon.m_innerPolylines.size(); j++) {
					VICUS::Polygon2D polygon2d(m_multiPolygon.m_innerPolylines[j]);
					holes.push_back(PlaneGeometry::Hole(j, polygon2d, true));
				}

				VICUS::PlaneGeometry& planeGeometry = geometryData.m_planeGeometry.back();
				planeGeometry.setHoles(holes);
			}

			geometryData.m_color = m_colorArea;
			m_geometryData.push_back(geometryData);

			m_dirtyTriangulation = false;
		}
		for(auto& geometryData : m_geometryData) {
			data.push_back(&geometryData);
		}
	}
	catch (IBK::Exception &ex) {
		throw IBK::Exception( ex, IBK::FormatString("Error generating plane geometries for 'DrawingOSM::AreaBorder' element.\n%1").arg(ex.what()), FUNC_ID);
	}
}

const void DrawingOSM::Building::addGeometryData(std::vector<VICUS::DrawingOSM::GeometryData*> &data) const
{
	for (auto& areaBorder : m_areaBorders) {
		areaBorder.addGeometryData(data);
	}
}

const void DrawingOSM::Highway::addGeometryData(std::vector<GeometryData *> & data) const
{
	for (auto& lineFromPlanes : m_linesFromPlanes) {
		lineFromPlanes.addGeometryData(data);
	}
	for (auto& areaBorder : m_areaBorders) {
		areaBorder.addGeometryData(data);
	}
}

const void DrawingOSM::AreaNoBorder::addGeometryData(std::vector<GeometryData *> & data) const
{
	FUNCID(DrawingOSM::AreaNoBorder::addGeometryData);
	try {
		if (m_dirtyTriangulation) {
			m_geometryData.clear();

			std::vector<IBKMK::Vector3D> areaPoints;

			for (int i = 1; i < m_multiPolygon.m_outerPolyline.size(); i++) {
				IBKMK::Vector3D p = IBKMK::Vector3D(m_multiPolygon.m_outerPolyline[i].m_x,
													m_multiPolygon.m_outerPolyline[i].m_y,
													0);

				QVector3D vec = m_drawing->m_rotationMatrix.toQuaternion() * IBKVector2QVector(p);
				vec += IBKVector2QVector(m_drawing->m_origin);

				areaPoints.push_back(QVector2IBKVector(vec));
			}

			VICUS::Polygon3D polygon3D(areaPoints);
			GeometryData geometryData;
			// Initialize PlaneGeometry with the polygon
			geometryData.m_planeGeometry.push_back(VICUS::PlaneGeometry(polygon3D));

			if(!m_multiPolygon.m_innerPolylines.empty()) {
				std::vector<PlaneGeometry::Hole> holes;
				for(int j = 0; j < m_multiPolygon.m_innerPolylines.size(); j++) {
					VICUS::Polygon2D polygon2d(m_multiPolygon.m_innerPolylines[j]);
					holes.push_back(PlaneGeometry::Hole(j, polygon2d, true));
				}

				VICUS::PlaneGeometry& planeGeometry = geometryData.m_planeGeometry.back();
				planeGeometry.setHoles(holes);
			}

			geometryData.m_color = m_color;
			m_geometryData.push_back(geometryData);

			m_dirtyTriangulation = false;
		}
		for (auto& geometryData : m_geometryData) {
			data.push_back(&geometryData);
		}
	}
	catch (IBK::Exception &ex) {
		throw IBK::Exception( ex, IBK::FormatString("Error generating plane geometries for 'DrawingOSM::AreaNoBorder' element.\n%1").arg(ex.what()), FUNC_ID);
	}
}

const void DrawingOSM::LineFromPlanes::addGeometryData(std::vector<GeometryData *> & data) const
{
	FUNCID(DrawingOSM::LineFromPlanes::addGeometryData);
	try {
		if (m_dirtyTriangulation) {

			// Create Vector to store vertices of polyline
			std::vector<IBKMK::Vector3D> polylinePoints;

			// adds z-coordinate to polyline
			for(unsigned int i = 0; i < m_polyline.size(); ++i){
				IBKMK::Vector3D p = IBKMK::Vector3D(m_polyline[i].m_x,
													m_polyline[i].m_y,
													0);
				QVector3D vec = m_drawing->m_rotationMatrix.toQuaternion() * IBKVector2QVector(p);
				vec += IBKVector2QVector(m_drawing->m_origin);

				polylinePoints.push_back(QVector2IBKVector(vec));
			}

			std::vector<PlaneGeometry> planeGeometry;
			if (m_drawing->generatePlanesFromPolyline(polylinePoints, false, m_lineThickness, planeGeometry)) {
				GeometryData geometryData;
				geometryData.m_planeGeometry = planeGeometry;
				geometryData.m_color = m_color;
				m_geometryData.push_back(geometryData);
			}
		}
		for (auto& geometryData : m_geometryData) {
			data.push_back(&geometryData);
		}
	}
	catch (IBK::Exception &ex) {
		throw IBK::Exception( ex, IBK::FormatString("Error generating plane geometries for 'DrawingOSM::LineFromPlanes' element.\n%1").arg(ex.what()), FUNC_ID);
	}
}

const void DrawingOSM::Water::addGeometryData(std::vector<GeometryData *> & data) const
{
	for (auto& areaNoBorder : m_areaNoBorders) {
		areaNoBorder.addGeometryData(data);
	}
	for (auto& lineFromPlanes : m_linesFromPlanes) {
		lineFromPlanes.addGeometryData(data);
	}
}

const void DrawingOSM::Land::addGeometryData(std::vector<GeometryData *> & data) const
{
	for (auto& areaNoBorder : m_areaNoBorders) {
		areaNoBorder.addGeometryData(data);
	}
}

const void DrawingOSM::Leisure::addGeometryData(std::vector<GeometryData *> & data) const
{
	for (auto& areaNoBorder : m_areaNoBorders) {
		areaNoBorder.addGeometryData(data);
	}
}

const void DrawingOSM::Natural::addGeometryData(std::vector<GeometryData *> & data) const
{
	for (auto& areaNoBorder : m_areaNoBorders) {
		areaNoBorder.addGeometryData(data);
	}
	for (auto& lineFromPlanes : m_linesFromPlanes) {
		lineFromPlanes.addGeometryData(data);
	}
}

const void DrawingOSM::Amenity::addGeometryData(std::vector<GeometryData *> & data) const
{
	for (auto& areaNoBorder : m_areaNoBorders) {
		areaNoBorder.addGeometryData(data);
	}
}

const void DrawingOSM::Place::addGeometryData(std::vector<GeometryData *> & data) const
{
	for (auto& areaBorder : m_areaBorders) {
		areaBorder.addGeometryData(data);
	}
}


void DrawingOSM::AbstractOSMObject::initialize(AbstractOSMElement & osmElement)
{
	std::string layer = osmElement.getValueFromKey("layer");
	if (layer != "") m_layer = std::stoi(layer);
}

} // namespace VICUS
