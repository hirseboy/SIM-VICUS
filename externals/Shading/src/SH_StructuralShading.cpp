/*	The Thermal Room Model
	Copyright (C) 2010  Andreas Nicolai <andreas.nicolai -[at]- tu-dresden.de>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "SH_StructuralShading.h"


#include <IBK_messages.h>
#include <IBK_FormatString.h>
#include <IBK_CSVReader.h>
#include <IBK_UnitVector.h>
#include <IBK_physics.h>

#include <IBKMK_Vector3D.h>

#include <CCM_SunPositionModel.h>
#include <CCM_SolarRadiationModel.h>

#include <DATAIO_DataIO.h>

#include <cmath>

#include "SH_ShadedSurfaceObject.h"

#if defined(_OPENMP)
#include <omp.h> // needed for omp_get_num_threads()
#endif

namespace SH {

/*! Returns Angle between vectors in DEG */
static double angleVectors(const IBKMK::Vector3D &v1, const IBKMK::Vector3D &v2) {
	double dot = v1.scalarProduct(v2);    // between [x1, y1, z1] and [x2, y2, z2]
	double angle = std::acos( dot/sqrt(v1.magnitude() * v2.magnitude() ) ) / IBK::DEG2RAD;

	return angle;
}


void StructuralShading::initializeShadingCalculation(int timeZone, double longitudeInDeg, double latitudeInDeg,
									const IBK::Time & startTime, unsigned int duration, unsigned int samplingPeriod,
									double sunConeDeg)
{
	// TODO Stephan, validity checks, throw IBK::Exception if out of range
	m_timeZone =  timeZone;
	m_longitudeInDeg = longitudeInDeg;
	m_latitudeInDeg = latitudeInDeg;

	m_startTime = startTime;
	m_duration = duration;
	m_samplingPeriod = samplingPeriod;

	m_sunConeDeg = sunConeDeg;

	IBK_ASSERT(m_samplingPeriod > 0);
	// TODO : issue warning, if duration/samplingPeriod has reminder

	// create a vector with sun positions for sampling periods
	createSunNormals();
}


void StructuralShading::setGeometry(const std::vector<std::vector<IBKMK::Vector3D> > &surfaces, const std::vector<std::vector<IBKMK::Vector3D> > &obstacles) {
	m_surfaces.clear();
	m_obstacles.clear();

	// first we set our obstacles
		for (const std::vector<IBKMK::Vector3D> &polyline : surfaces) {
			m_surfaces.push_back( IBKMK::Polygon3D(polyline) );
		}
		for (const std::vector<IBKMK::Vector3D> &polyline : obstacles) {
			m_obstacles.push_back( IBKMK::Polygon3D(polyline) );
		}
		// error checking is done in setGeometry()
		setGeometry(m_surfaces, m_obstacles);
}


void StructuralShading::setGeometry(const std::vector<IBKMK::Polygon3D> & surfaces, const std::vector<IBKMK::Polygon3D> & obstacles) {
	FUNCID(StructuralShading::setGeometry);
	try {
		m_surfaces = surfaces;
		m_obstacles = obstacles;

		for (IBKMK::Polygon3D & p : m_surfaces) {
			if (!p.isValid())
				throw IBK::Exception(IBK::FormatString("Polygon is not valid."), FUNC_ID);
		}
		for (IBKMK::Polygon3D & p : m_obstacles) {
			if (!p.isValid())
				throw IBK::Exception(IBK::FormatString("Polygon is not valid."), FUNC_ID);
		}

	}
	catch (IBK::Exception &ex) {
		throw IBK::Exception(ex, IBK::FormatString("Could not set geometry for calculation!"), FUNC_ID);
	}
}


void StructuralShading::calculateShadingFactors(Notification * notify, double gridWidth) {
	FUNCID(StructuralShading::calculateShadingFactors);

	// TODO Stephan, input data check

	m_gridWidth = gridWidth;

	// prepare target memory
	m_shadingFactors.resize(m_sunConeNormals.size());
	for (std::vector<double> & sf : m_shadingFactors) {
		sf.resize(m_surfaces.size(), 0); // fully shaded, i.e. default since we do not handle night-time
	}

#if defined(_OPENMP)
#pragma omp parallel
	{
		if (omp_get_thread_num() == 0)
			IBK::IBK_Message(IBK::FormatString("Num threads = %1").arg(omp_get_num_threads()));
	}
#endif

#if defined(_OPENMP)
#pragma omp parallel for
#endif
	for (int surfCounter = 0; surfCounter < (int)m_surfaces.size(); ++surfCounter) {
		if (notify->m_aborted)
			continue;

		// readability improvement
		const IBKMK::Polygon3D & surf = m_surfaces[(unsigned int)surfCounter];

		// 1. split polygon 'surf' into sub-polygons based on grid information and compute center point of these sub-polygons

		ShadedSurfaceObject surfaceObject;
		surfaceObject.setPolygon(surf, m_gridWidth);

		// 2. for each center point perform intersection tests again _all_ obstacle polygons

		for (unsigned int i=0; i<m_sunConeNormals.size(); ++i) {

			double sf = surfaceObject.calcShadingFactor(m_sunConeNormals[i], m_obstacles);

			// 3. store shaded/not shaded information for sub-polygon and its surface area
			// 4. compute area-weighted sum of shading factors and devide by orginal polygon surface
			// 5. store in result vector
			m_shadingFactors[i][(unsigned int)surfCounter] = sf;
		}
	} // omp for loop

}

void StructuralShading::writeShadingFactorsToTSV(const IBK::Path & path) {
	FUNCID(StructuralShading::shadingFactorsTSV);

	std::vector< double >			   timePoints;
	std::vector< std::vector<double> > data;

	if (!path.isValid())
		throw IBK::Exception("Invalid filename or no filename set.", FUNC_ID);

	// check for correct file extension
	std::string ext = path.extension();
	// only remove extension if d6b or d6o is used
	IBK::Path fname_wo_ext = path;
	if (ext == "tsv")
		fname_wo_ext = path.withoutExtension();
	fname_wo_ext.addExtension(".tsv");

	if (fname_wo_ext != path) {
		throw IBK::Exception(IBK::FormatString("Invalid filename (extension) of output file '%1', should have been '%2'!\n")
			.arg(path).arg(fname_wo_ext), FUNC_ID);
	}

	std::ofstream tsvFile ( path.str() );

	if ( !tsvFile.is_open() )
		throw IBK::Exception(IBK::FormatString("Could not open output file '%1'\n").arg(path.str() ), FUNC_ID );
#if 0
	// write header line
	tsvFile << "Time [h]\t"; // write header
	for ( unsigned int i=0; i<m_surfaces.size(); ++i ) {
		tsvFile << m_surfaces[i].m_id << " [---]\t";
	}
	tsvFile << "\n";

	for( unsigned int i=0; i<m_sunPositions.size(); ++i) {
		tsvFile << IBK::val2string<int>(m_sunPositions[i].m_secOfYear/3600) << "\t";
		for ( unsigned int j=0; j<m_surfaces.size(); ++j ) {
			const Polygon &poly = m_surfaces[j];

			tsvFile	<< poly.m_shadingFactors.value(m_sunPositions[i].m_secOfYear) << "\t";
		}
		tsvFile << "\n";
	}
#endif
	tsvFile.close();
}

void StructuralShading::writeShadingFactorsToDataIO(const IBK::Path & path, bool isBinary) {

	// We create a dataIO container
	DATAIO::DataIO dataContainer;
	dataContainer.m_filename = path;
	dataContainer.m_isBinary = isBinary;
	dataContainer.m_type = DATAIO::DataIO::T_REFERENCE;
	dataContainer.m_spaceType = DATAIO::DataIO::ST_SINGLE;
	dataContainer.m_timeType = DATAIO::DataIO::TT_NONE;
	dataContainer.m_timeUnit = "h";

	std::vector< double > timePoints ( m_sunPositions.size() ) ;
	std::vector< std::vector<double> > values ( m_sunPositions.size(), std::vector<double> (m_surfaces.size() ) );
	std::vector<unsigned int> nums;
	std::string quantity;

	for( unsigned int i=0; i<m_sunPositions.size(); ++i) {
		timePoints[i] = m_sunPositions[i].m_secOfYear; // mind that we need time points in seconds
	}
#if 0
	for( unsigned int j=0; j<m_sunPositions.size(); ++j) {
		for ( unsigned int i=0; i<m_surfaces.size(); ++i ) {
			const IBKMK::Polygon3D &poly = m_surfaces[i];
			if ( j == 0 ) {
				nums.emplace_back(poly.m_id);
				quantity += IBK::val2string<unsigned int>(poly.m_id);
				if ( i < m_surfaces.size() - 1 )
					quantity += " | ";
			}
			values[j][i] = poly.m_shadingFactors.value(m_sunPositions[j].m_secOfYear);
		}
	}
#endif
	// we set our data
	dataContainer.m_nums = nums;
	dataContainer.m_quantity = quantity;
	dataContainer.m_valueUnit = "---";
	dataContainer.setData(timePoints, values);
	dataContainer.write();
}


void StructuralShading::createSunNormals() {

	// *** first calculation sun positions for each sampling interval

	m_sunPositions.clear();

	CCM::SunPositionModel sunModel;

	sunModel.m_latitude = m_latitudeInDeg * IBK::DEG2RAD;
	sunModel.m_longitude = m_longitudeInDeg * IBK::DEG2RAD;

	unsigned int startStep = (unsigned int)std::floor(m_startTime.secondsOfYear()/m_samplingPeriod);
	unsigned int stepCount = (unsigned int)std::ceil(m_duration/m_samplingPeriod);

	for (unsigned int i=0; i<stepCount; ++i) {
		unsigned int timeInSec = startStep + i*m_samplingPeriod;
		double localMeanTime = CCM::SolarRadiationModel::localMeanTimeFromLocalStandardTime(timeInSec, m_timeZone, m_longitudeInDeg);
		double apparentTime = CCM::SolarRadiationModel::apparentSolarTimeFromLocalMeanTime(localMeanTime);
		sunModel.setTime(apparentTime);		//sun position at middle of the hour; hourly interval

		m_sunPositions.emplace_back( SunPosition( timeInSec, sunModel.m_azimuth, sunModel.m_elevation ) );
	}

	// *** compute sun normals (clustered)

	m_sunConeNormals.clear();
	m_sunConeNormalTimePoints.clear();

	/// We initialize all our coresponding sun normals.
	///
	/// For each sampling interval we first compute the corresponding sun' normal vector.
	/// Then, we search through our vector of previously computed sun normals and check if we are close enough.
	/// This is done in function findSimilarNormal(). This function returns the index of the existing normal vector
	/// if we are close enough, or -1 if we are too far away or -2 if sun is beyond horizon
	///
	/// NOTE: in angles around 90 Deg between our surface normal and the sun beam
	/// we can get faulty shading factors. But since radiation loads are also
	/// low, we accept this. When the sun cone angle gets lower (eg 1 Deg) we
	/// minimize this problem

	for (size_t i=0; i<m_sunPositions.size(); ++i) {
		IBKMK::Vector3D n = m_sunPositions[i].calcNormal();
		int id = findSimilarNormals(n);

		// record new different sun normal
		if (id == -1) {
			m_sunConeNormals.push_back(n);
			m_sunConeNormalTimePoints.push_back(startStep + i*m_samplingPeriod);
			m_indexesOfSimilarNormals.push_back( std::vector<unsigned int>(1, i) );
		}
		else if (id > 0) {
			// similar normal exists, append current sun positions index to vector
			m_indexesOfSimilarNormals[(unsigned int)id].push_back(i);
		}
	}

}


int StructuralShading::findSimilarNormals(const IBKMK::Vector3D &sunNormal) const {

	// if sun is not above horizon
	if (sunNormal.m_z < 0)
		return -2;

	// search through all previous computed normals and check if angle between normals is below threshold
	for (unsigned int i=0; i<m_sunConeNormals.size(); ++i) {
		double diffAngle = angleVectors(m_sunConeNormals[i], sunNormal);
		if (std::fabs(diffAngle) <= m_sunConeDeg)
			return (int)i;
	}

	return -1;
}

} // namespace TH
