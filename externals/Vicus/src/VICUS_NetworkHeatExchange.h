/*	The SIM-VICUS data model library.

	Copyright (c) 2020-today, Institut für Bauklimatik, TU Dresden, Germany

	Primary authors:
	  Andreas Nicolai  <andreas.nicolai -[at]- tu-dresden.de>
	  Dirk Weiss  <dirk.weiss -[at]- tu-dresden.de>
	  Stephan Hirth  <stephan.hirth -[at]- tu-dresden.de>
	  Hauke Hirsch  <hauke.hirsch -[at]- tu-dresden.de>

	  ... all the others from the SIM-VICUS team ... :-)

	This library is part of SIM-VICUS (https:/gfx/modeltypeicons//github.com/ghorwin/SIM-VICUS)

	This library is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
*/

#ifndef VICUS_NETWORKHEATEXCHANGE_H
#define VICUS_NETWORKHEATEXCHANGE_H

#include "VICUS_CodeGenMacros.h"
#include "VICUS_Constants.h"
#include "VICUS_AbstractDBElement.h"
#include "VICUS_Schedule.h"

#include <NANDRAD_HydraulicNetworkHeatExchange.h>

#include <IBK_Parameter.h>

namespace VICUS {

/*! Encapsulates all data defining heat exchange between flow elements and
	the environment or other models/elements.

	Definition of heat exchange is done in each flow element definition. If missing, the flow
	element is treated as adiabat.
*/
class NetworkHeatExchange
{
public:
	NetworkHeatExchange() = default;

	/*! Defines the type of heat exchange */
	enum ModelType {
		T_TemperatureConstant,				// Keyword: TemperatureConstant			'Constant temperature'
		T_TemperatureSpline,				// Keyword: TemperatureSpline			'Time-dependent temperature'
		T_TemperatureConstantEvaporator,	// Keyword: TemperatureConstantEvaporator 'Constant source temperature'
		T_TemperatureSplineEvaporator,		// Keyword: TemperatureSplineEvaporator	'Time-dependent source temperature'
		T_TemperatureZone,					// Keyword: TemperatureZone				'Zone air temperature'
		T_TemperatureConstructionLayer,		// Keyword: TemperatureConstructionLayer 'Active construction layer (floor heating)'
		T_HeatLossConstant,					// Keyword: HeatLossConstant			'Constant heat loss'
		T_HeatLossSpline,					// Keyword: HeatLossSpline				'Time-dependent heat loss'
		/*! Heat loss from condenser is not the heat loss of the fluid, hence different parameter than T_HeatLossSpline. */
		T_HeatLossConstantCondenser,		// Keyword: HeatLossConstantCondenser	'Constant heating demand'
		T_HeatLossSplineCondenser,			// Keyword: HeatLossSplineCondenser		'Time-dependent heating demand'
		/*! Heating demand for space heating  */
		T_HeatingDemandSpaceHeating,		// Keyword: HeatingDemandSpaceHeating	'Heating demand for space heating'
		NUM_T
	};

	/*! Parameters for the element . */
	enum para_t {
		/*! Basic parameters - identical to NANDRAD::HydraulicNetworkHeatExchange */
		P_Temperature,							// Keyword: Temperature							[C]		'Temperature for heat exchange'
		P_HeatLoss,								// Keyword: HeatLoss							[W]		'Constant heat flux out of the element (heat loss)'
		P_ExternalHeatTransferCoefficient,		// Keyword: ExternalHeatTransferCoefficient		[W/m2K]	'External heat transfer coeffient for the outside boundary'
		/*! parameters for GUI */
		P_FloorArea,							// Keyword: FloorArea							[m2]		'FloorArea'
		P_MaximumHeatingLoad,					// Keyword: MaximumHeatingLoad					[kW]		'MaximumHeatingLoad'
		P_HeatingEnergyDemand,					// Keyword: HeatingEnergyDemand					[kWh]		'HeatingEnergyDemand'
		P_MaximumCoolingLoad,					// Keyword: MaximumCoolingLoad					[kW]		'MaximumCoolingLoad'
		P_CoolingEnergyDemand,					// Keyword: CoolingEnergyDemand					[kWh]		'CoolingEnergyDemand'
		P_DomesticHotWaterDemand,				// Keyword: DomesticHotWaterDemand				[kWh]		'DomesticHotWaterDemand'
		P_HeatingSupplyTemperature,				// Keyword: HeatingSupplyTemperature			[C]			'HeatingSupplyTemperature'
		P_DomesticHotWaterSupplyTemperature,	// Keyword: DomesticHotWaterSupplyTemperature	[C]			'DomesticHotWaterSupplyTemperature'
		NUM_P
	};

	enum BuildingType {
		BT_ResidentialBuildingSingleFamily,		// Keyword: ResidentialBuildingSingleFamily		'Single Family House'
		BT_ResidentialBuildingMultiFamily,		// Keyword: ResidentialBuildingMultiFamily		'Multi Family House'
		BT_ResidentialBuildingLarge,			// Keyword: ResidentialBuildingLarge			'Large Residential Building'
		BT_OfficeBuilding,						// Keyword: OfficeBuilding						'Office Building'
		BT_UserDefineBuilding,					// Keyword: UserDefineBuilding					'User Defined Building'
		NUM_BT
	};

	enum AmbientTemperatureType {
		AT_UndisturbedSoilTemperature,			// Keyword: UndisturbedSoilTemperature			'Undisturbed Soil Temperature'
		AT_BoreholeHeatExchangerTemperature,	// Keyword: BoreholeHeatExchanger				'Borehole Heat Exchanger'
		AT_UserDefined,							// Keyword: UserDefined							'User Defined'
		NUM_AT
	};

	// *** PUBLIC MEMBER FUNCTIONS ***

	VICUS_READWRITE

	/*! Sets default values for all parameters, depending on model type */
	void setDefaultValues(ModelType modelType);

	/*! Read all tsv files */
	void readPredefinedTSVFiles() const;

	/*! Wrap file reading */
	void readSingleTSVFile(QString filename, std::vector<double> &yData) const;

	void checkAndInitializeSpline() const;

	bool generateHeatingDemandSpline(std::vector<double> &spline, double k=-1) const;

	bool generateCoolingDemandSpline(std::vector<double> &spline, double k=-1) const;

	void generateDHWDemandSpline(std::vector<double> &spline) const;

	void calculateHeatLossSplineFromKValue(double k, const std::vector<double> & original, std::vector<double> & result, double maxValueRequired, double maxValueExisting=-1) const;

	bool calculateNewKValue(const std::vector<double> & original, double integralRequired, double maxValueRequired, double & integralResult, double & KResult) const;

	Schedule condenserTemperatureSchedule() const;

	Schedule evaporatorTemperatureSchedule() const;

	NANDRAD::HydraulicNetworkHeatExchange toNandradHeatExchange() const;

	// *** Public Member variables ***

	/*! Model Type */
	ModelType							m_modelType		= NUM_T;					// XML:A

	bool								m_individualHeatExchange = false;			// XML:A

	bool								m_areaRelatedValues = false;				// XML:A

	bool								m_withCoolingDemand = true;					// XML:A

	bool								m_withDomesticHotWaterDemand = false;		// XML:A

	bool								m_useHeatTransferCoefficient = false; 		// XML:A

	BuildingType						m_buildingType = BT_ResidentialBuildingSingleFamily; // XML:A

	AmbientTemperatureType				m_ambientTemperatureType = NUM_AT;			// XML:A

	NANDRAD::LinearSplineParameter		m_userDefinedHeatFlux;						// XML:E

	/*! Parameters for the element . */
	IBK::Parameter						m_para[NUM_P];								// XML:E

	/*! IBK::Path to user defined tsv-file to be displayed in the plot */
	IBK::Path							m_userDefinedTsvFile;						// XML:E

	// *** Run time variables
	mutable std::vector<double>					m_heatingDemandSplineOrig;
	mutable std::vector<double>					m_coolingDemandSplineOrig;
	mutable std::vector<unsigned int>			m_timePointsDHWPreparation;

	mutable std::vector<double>					m_temperatureSplineY;
	mutable NANDRAD::LinearSplineParameter		m_exportSpline;

};

} // namespace VICUS

#endif // VICUS_NETWORKHEATEXCHANGE_H
