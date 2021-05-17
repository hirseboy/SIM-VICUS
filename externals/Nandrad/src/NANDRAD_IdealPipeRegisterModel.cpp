#include "NANDRAD_IdealPipeRegisterModel.h"

#include "NANDRAD_KeywordList.h"
#include "NANDRAD_Zone.h"

#include <algorithm>

namespace NANDRAD {

void IdealPipeRegisterModel::checkParameters(const std::vector<NANDRAD::Zone> &zones) {
	FUNCID(IdealPipeRegisterModel::checkParameters);
	// check parameters
	m_para[P_MaxMassFlow].checkedValue("MaxMassFlow", "kg/s", "kg/s",
											   0, false,
											   std::numeric_limits<double>::max(), true,
											   "Maximum mass flow must be > 0 kg/s.");
	m_para[P_PipeLength].checkedValue("PipeLength", "m", "m",
											   0, false,
											   std::numeric_limits<double>::max(), true,
											   "Pipe lenght must be > 0 m.");
	m_para[P_PipeInnerDiameter].checkedValue("PipeInnerDiameter", "mm", "mm",
											   0, false,
											   std::numeric_limits<double>::max(), true,
											   "Pipe inner diameter flow must be > 0 kg/s.");
	m_para[P_UValuePipeWall].checkedValue("UValuePipeWall", "W/mK", "W/mK",
											   0, false,
											   std::numeric_limits<double>::max(), true,
											   "Pipe wall U-value must be > 0 W/mK.");
	m_para[P_FluidDensity].checkedValue("FluidDensity", "kg/m3", "kg/m3",
											   0, false,
											   std::numeric_limits<double>::max(), true,
											   "Fluid density must be > 0 kg/m3.");
	m_para[P_FluidHeatCapacity].checkedValue("FluidHeatCapacity", "J/kgK", "J/kgK",
											   0, false,
											   std::numeric_limits<double>::max(), true,
											   "Fluid heat capacity must be > 0 J/kgK.");
	m_para[P_FluidConductivity].checkedValue("FluidConductivity", "W/mK", "W/mK",
											   0, false,
											   std::numeric_limits<double>::max(), true,
											   "Fluid conductivity must be > 0 W/mK.");

	// decide how to recieve supply temperature
	if(m_modelType == MT_Constant) {
		m_para[P_SupplyTemperature].checkedValue("SupplyTemperature", "C", "K",
												   0.0, true,
												   std::numeric_limits<double>::max(), true,
												   "Supply temperature must be >= 0 K.");
	}

	// set default value for int parameters
	if(m_intPara[IP_NumberParallelPipes].name.empty()) {
		// one pipe is default
		m_intPara[IP_NumberParallelPipes].set("NumberParallelPipes", 1);
	}
	else {
		// prove value
		m_intPara[IP_NumberParallelPipes].checkIfValueIsAboveLimit(IBK::IntPara("NumberParallelPipes", 1), true);
	}

	// kinematic viscosity is always needed, here we check the spline and convert it to base units automatically
	m_fluidViscosity.checkAndInitialize("FluidViscosity", IBK::Unit("K"), IBK::Unit("m2/s"),
											IBK::Unit("m2/s"), 0, false, std::numeric_limits<double>::max(), false,
											"Fluid viscosity must be > 0 m2/s.");

	// check validity of thermostat zone
	std::vector<NANDRAD::Zone>::const_iterator zone_it = std::find(zones.begin(),
																  zones.end(),
																  m_thermostatZoneID);

	if (zone_it == zones.end())
		throw IBK::Exception(IBK::FormatString("Invalid/undefined zone with '%1' in ThermostatZoneId.")
							 .arg(m_thermostatZoneID), FUNC_ID);

}


} // namespace NANDRAD

