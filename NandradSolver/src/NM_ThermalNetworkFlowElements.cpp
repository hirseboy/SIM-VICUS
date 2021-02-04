#include "NM_ThermalNetworkFlowElements.h"
#include "NM_Physics.h"

#include "NANDRAD_HydraulicFluid.h"
#include "NANDRAD_HydraulicNetworkElement.h"
#include "NANDRAD_HydraulicNetworkPipeProperties.h"
#include "NANDRAD_HydraulicNetworkComponent.h"

#include "numeric"

#define PI				3.141592653589793238

namespace NANDRAD_MODEL {

// *** TNStaticPipeElement ***

TNStaticPipeElement::TNStaticPipeElement(const NANDRAD::HydraulicNetworkElement & elem,
							 const NANDRAD::HydraulicNetworkComponent & comp,
							const NANDRAD::HydraulicNetworkPipeProperties & pipePara,
							const NANDRAD::HydraulicFluid & fluid)
{
	m_length = elem.m_para[NANDRAD::HydraulicNetworkElement::P_Length].value;
	m_innerDiameter = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_PipeInnerDiameter].value;
	m_outerDiameter = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_PipeOuterDiameter].value;
	m_UValuePipeWall = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_UValuePipeWall].value;
	m_outerHeatTransferCoefficient =
			comp.m_para[NANDRAD::HydraulicNetworkComponent::P_ExternalHeatTransferCoefficient].value;
	// set a small volume
	m_fluidVolume = 0.01;
	// copy fluid properties
	m_fluidDensity = fluid.m_para[NANDRAD::HydraulicFluid::P_Density].value;
	m_fluidHeatCapacity = fluid.m_para[NANDRAD::HydraulicFluid::P_HeatCapacity].value;
	m_fluidConductivity = fluid.m_para[NANDRAD::HydraulicFluid::P_Conductivity].value;
}

TNStaticPipeElement::~TNStaticPipeElement()
{

}

void TNStaticPipeElement::setNodalConditions(double mdot, double TInlet, double TOutlet)
{
	// TODO: add initialization
	const double viscosity = 3e-05;
	const double ambientTemperature = 273.15;

	// copy mass flux
	ThermalNetworkAbstractFlowElementWithHeatLoss::setNodalConditions(mdot, TInlet, TOutlet);

	// calculate inner heat transfer coefficient
	const double velocity = std::fabs(m_massFlux)/(m_fluidVolume * m_fluidDensity);
//	const double viscosity = m_fluid->m_kinematicViscosity.m_values.value(m_temperature);
	const double reynolds = ReynoldsNumber(velocity, viscosity, m_innerDiameter);
	const double prandtl = PrandtlNumber(viscosity, m_fluidHeatCapacity, m_fluidConductivity, m_fluidDensity);
	double nusselt = NusseltNumber(reynolds, prandtl, m_length, m_innerDiameter);
	double innerHeatTransferCoefficient = nusselt * m_fluidConductivity /
											m_innerDiameter;

	// calculate heat transfer
	const double UAValueTotal = (PI*m_length) / (1.0/(innerHeatTransferCoefficient * m_innerDiameter)
													+ 1.0/(m_outerHeatTransferCoefficient * m_outerDiameter)
													+ 1.0/(2.0*m_UValuePipeWall) );

	if(m_massFlux >= 0) {
		// calculate heat loss with given parameters
		m_heatLoss = m_massFlux * m_fluidHeatCapacity *
				(m_inletTemperature - ambientTemperature) *
				(1. - std::exp(-UAValueTotal / (std::fabs(m_massFlux) * m_fluidHeatCapacity )));
	}
	else {
		// calculate heat loss with given parameters
		m_heatLoss = std::fabs(m_massFlux) * m_fluidHeatCapacity *
				(m_outletTemperature - ambientTemperature) *
				(1. - std::exp(-UAValueTotal / (std::fabs(m_massFlux) * m_fluidHeatCapacity )));
	}
}


// *** TNStaticAdiabaticPipeElement ***

TNStaticAdiabaticPipeElement::TNStaticAdiabaticPipeElement(const NANDRAD::HydraulicNetworkElement & /*elem*/,
							 const NANDRAD::HydraulicNetworkComponent & /*comp*/,
							const NANDRAD::HydraulicNetworkPipeProperties & /*pipePara*/,
							const NANDRAD::HydraulicFluid & fluid)
{
	// set a small volume
	m_fluidVolume = 0.01;
	// copy fluid properties
	m_fluidDensity = fluid.m_para[NANDRAD::HydraulicFluid::P_Density].value;
	m_fluidHeatCapacity = fluid.m_para[NANDRAD::HydraulicFluid::P_HeatCapacity].value;
}

TNStaticAdiabaticPipeElement::~TNStaticAdiabaticPipeElement()
{

}


// *** DynamicPipeElement ***

TNDynamicPipeElement::TNDynamicPipeElement(const NANDRAD::HydraulicNetworkElement & elem,
							 const NANDRAD::HydraulicNetworkComponent & comp,
							const NANDRAD::HydraulicNetworkPipeProperties & pipePara,
							const NANDRAD::HydraulicFluid & fluid)
{
	m_length = elem.m_para[NANDRAD::HydraulicNetworkElement::P_Length].value;
	m_innerDiameter = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_PipeInnerDiameter].value;
	m_outerDiameter = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_PipeOuterDiameter].value;
	m_UValuePipeWall = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_UValuePipeWall].value;
	m_outerHeatTransferCoefficient =
			comp.m_para[NANDRAD::HydraulicNetworkComponent::P_ExternalHeatTransferCoefficient].value;
	// calculate fluid volume inside the pipe
	m_fluidVolume = PI/4. * m_innerDiameter * m_innerDiameter * m_length;
	// copy fluid properties
	m_fluidDensity = fluid.m_para[NANDRAD::HydraulicFluid::P_Density].value;
	m_fluidHeatCapacity = fluid.m_para[NANDRAD::HydraulicFluid::P_HeatCapacity].value;
	m_fluidConductivity = fluid.m_para[NANDRAD::HydraulicFluid::P_Conductivity].value;

	// caluclate discretization
	double minDiscLength = comp.m_para[NANDRAD::HydraulicNetworkComponent::P_PipeMaxDiscretizationWidth].value;
	IBK_ASSERT(minDiscLength < m_length);

	// claculte number of discretization elements
	m_nVolumes = (unsigned int) (m_length/minDiscLength);
	// resize all vectors
	m_specificEnthalpies.resize(m_nVolumes, 0.0);
	m_temperatures.resize(m_nVolumes, 273.15);
	m_heatLosses.resize(m_nVolumes, 0.0);

	// calculate segment specific quantities
	m_discLength = m_length/(double) m_nVolumes;
	m_discVolume = m_fluidVolume/(double) m_nVolumes;
}

TNDynamicPipeElement::~TNDynamicPipeElement()
{

}

unsigned int TNDynamicPipeElement::nInternalStates() const
{
	return m_nVolumes;
}

void TNDynamicPipeElement::setInitialTemperature(double T0) {
	// use standard implementation
	ThermalNetworkAbstractFlowElementWithHeatLoss::setInitialTemperature(T0);
	// fill vector valued quantiteis
	std::fill(m_temperatures.begin(), m_temperatures.end(), T0);
	// calculte specific enthalpy
	double specificEnthalpy = T0 * m_fluidHeatCapacity;
	std::fill(m_specificEnthalpies.begin(), m_specificEnthalpies.end(), specificEnthalpy);
}

void TNDynamicPipeElement::initialInternalStates(double * y0) {
	// copy internal states
	for(unsigned int i = 0; i < m_nVolumes; ++i)
		y0[i] = m_specificEnthalpies[i] * m_discVolume * m_fluidDensity;
}

void TNDynamicPipeElement::setInternalStates(const double * y)
{
	double temp = 0.0;
	// calculate specific enthalpy
	for(unsigned int i = 0; i < m_nVolumes; ++i) {
		m_specificEnthalpies[i] = y[i] / ( m_discVolume * m_fluidDensity);
		m_temperatures[i] = m_specificEnthalpies[i] / m_fluidHeatCapacity;
		temp += m_temperatures[i];
	}
	m_meanTemperature = temp/(double) m_nVolumes;
}

void TNDynamicPipeElement::internalDerivatives(double * ydot)
{
	// heat fluxes into the fluid and enthalpy change are heat sources
	if(m_massFlux >= 0.0) {
		// first element copies boundary conditions
		ydot[0] = -m_heatLosses[0] + m_massFlux * (m_inletSpecificEnthalpy - m_specificEnthalpies[0]);
		for(unsigned int i = 1; i < m_nVolumes; ++i) {
			ydot[i] = -m_heatLosses[i] + m_massFlux * (m_specificEnthalpies[i - 1] - m_specificEnthalpies[i]);
		}
	}
	else { // m_massFlux < 0
		// last element copies boundary conditions
		ydot[m_nVolumes - 1] = -m_heatLosses[m_nVolumes - 1] + m_massFlux * (m_specificEnthalpies[m_nVolumes - 1]
				- m_outletSpecificEnthalpy);
		for(unsigned int i = 0; i < m_nVolumes - 1; ++i) {
			ydot[i] = -m_heatLosses[i] + m_massFlux * (m_specificEnthalpies[i] - m_specificEnthalpies[i + 1]);
		}
	}
}


void TNDynamicPipeElement::setNodalConditions(double mdot, double TInlet, double TOutlet)
{
	// TODO: add initialization
	const double viscosity = 3e-05;
	const double ambientTemperature = 273.15;

	ThermalNetworkAbstractFlowElementWithHeatLoss::setNodalConditions(mdot, TInlet, TOutlet);

	m_heatLoss = 0.0;

	// assume constant heat transfer coefficient along pipe, using average temperature
	const double velocity = std::fabs(mdot)/(m_fluidVolume * m_fluidDensity);
	const double avgTemperature = std::accumulate(m_temperatures.begin(), m_temperatures.end(), 0) /
								(double) m_temperatures.size();
	const double reynolds = ReynoldsNumber(velocity, viscosity, m_innerDiameter);
	const double prandtl = PrandtlNumber(viscosity, m_fluidHeatCapacity, m_fluidConductivity, m_fluidDensity);
	double nusselt = NusseltNumber(reynolds, prandtl, m_length, m_innerDiameter);
	double innerHeatTransferCoefficient = nusselt * m_fluidConductivity /
											m_innerDiameter;

	// UA value for one volume
	const double UAValue = (PI*m_discLength) / (1.0/(innerHeatTransferCoefficient * m_innerDiameter)
												+ 1.0/(m_outerHeatTransferCoefficient * m_outerDiameter)
												+ 1.0/(2.0*m_UValuePipeWall) );

	for(unsigned int i = 0; i < m_nVolumes; ++i) {
		// calculate heat loss with given parameters
		m_heatLosses[i] = UAValue * (m_temperatures[i] - ambientTemperature);
		// sum up heat losses
		m_heatLoss += m_heatLosses[i];
	}
}


void TNDynamicPipeElement::dependencies(const double *ydot, const double *y,
										const double *mdot, const double* TInlet, const double*TOutlet,
										const double *Qdot,
										std::vector<std::pair<const double *, const double *> > & resultInputDependencies) const {

	// set dependency to inlet and outlet enthalpy
	resultInputDependencies.push_back(std::make_pair(TInlet, y) );
	resultInputDependencies.push_back(std::make_pair(TOutlet, y + nInternalStates() - 1) );
	resultInputDependencies.push_back(std::make_pair(ydot, TInlet) );
	resultInputDependencies.push_back(std::make_pair(ydot + nInternalStates() - 1, TOutlet) );


	for(unsigned int n = 0; n < nInternalStates(); ++n) {

		// heat balance per default sums up heat fluxes and entahpy flux differences through the pipe
		if(n > 0)
			resultInputDependencies.push_back(std::make_pair(ydot + n, y + n - 1) );

		resultInputDependencies.push_back(std::make_pair(ydot + n, y + n) );

		if(n < nInternalStates() - 1)
			resultInputDependencies.push_back(std::make_pair(ydot + n, y + n + 1) );

		// set depedency to mdot
		resultInputDependencies.push_back(std::make_pair(ydot + n, mdot) );
		// set dependeny to Qdot
		resultInputDependencies.push_back(std::make_pair(Qdot, y + n) );
	}
}

// *** DynamicAdiabaticPipeElement ***

TNDynamicAdiabaticPipeElement::TNDynamicAdiabaticPipeElement(const NANDRAD::HydraulicNetworkElement & elem,
							 const NANDRAD::HydraulicNetworkComponent & comp,
							const NANDRAD::HydraulicNetworkPipeProperties & pipePara,
							const NANDRAD::HydraulicFluid & fluid)
{
	double length = elem.m_para[NANDRAD::HydraulicNetworkElement::P_Length].value;
	double innerDiameter = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_PipeInnerDiameter].value;

	// caluclate discretization
	double minDiscLength = comp.m_para[NANDRAD::HydraulicNetworkComponent::P_PipeMaxDiscretizationWidth].value;
	IBK_ASSERT(minDiscLength < length);

	// claculte number of discretization elements
	m_nVolumes = (unsigned int) (length/minDiscLength);
	// resize all vectors
	m_specificEnthalpies.resize(m_nVolumes, 0.0);
	m_temperatures.resize(m_nVolumes, 273.15);
	// calculate fluid volume inside the pipe
	m_fluidVolume = PI/4. * innerDiameter * innerDiameter * length;
	// copy fluid properties
	m_fluidDensity = fluid.m_para[NANDRAD::HydraulicFluid::P_Density].value;
	m_fluidHeatCapacity = fluid.m_para[NANDRAD::HydraulicFluid::P_HeatCapacity].value;

	// calculate segment specific quantities
	m_discVolume = m_fluidVolume/(double) m_nVolumes;
}

TNDynamicAdiabaticPipeElement::~TNDynamicAdiabaticPipeElement()
{

}

unsigned int TNDynamicAdiabaticPipeElement::nInternalStates() const
{
	return m_nVolumes;
}

void TNDynamicAdiabaticPipeElement::setInitialTemperature(double T0) {
	// use standard implementation
	ThermalNetworkAbstractFlowElement::setInitialTemperature(T0);
	// fill vector valued quantiteis
	std::fill(m_temperatures.begin(), m_temperatures.end(), T0);
	// calculte specific enthalpy
	double specificEnthalpy = T0 * m_fluidHeatCapacity;
	std::fill(m_specificEnthalpies.begin(), m_specificEnthalpies.end(), specificEnthalpy);
}

void TNDynamicAdiabaticPipeElement::initialInternalStates(double * y0) {
	// copy internal states
	for(unsigned int i = 0; i < m_nVolumes; ++i)
		y0[i] = m_specificEnthalpies[i] * m_discVolume * m_fluidDensity;
}

void TNDynamicAdiabaticPipeElement::setInternalStates(const double * y)
{
	double temp = 0.0;
	// calculate specific enthalpy
	for(unsigned int i = 0; i < m_nVolumes; ++i) {
		m_specificEnthalpies[i] = y[i] / ( m_discVolume * m_fluidDensity);
		m_temperatures[i] = m_specificEnthalpies[i] / m_fluidHeatCapacity;
		temp += m_temperatures[i];
	}
	m_meanTemperature = temp/(double) m_nVolumes;
}

void TNDynamicAdiabaticPipeElement::internalDerivatives(double * ydot)
{
	// heat fluxes into the fluid and enthalpy change are heat sources
	if(m_massFlux >= 0.0) {
		// first element copies boundary conditions
		ydot[0] = m_massFlux * (m_inletSpecificEnthalpy - m_specificEnthalpies[0]);
		for(unsigned int i = 1; i < m_nVolumes; ++i) {
			ydot[i] = m_massFlux * (m_specificEnthalpies[i - 1] - m_specificEnthalpies[i]);
		}
	}
	else { // m_massFlux < 0
		// last element copies boundary conditions
		ydot[m_nVolumes - 1] = m_massFlux * (m_specificEnthalpies[m_nVolumes - 1]
				- m_outletSpecificEnthalpy);
		for(unsigned int i = 0; i < m_nVolumes - 1; ++i) {
			ydot[i] = m_massFlux * (m_specificEnthalpies[i] - m_specificEnthalpies[i + 1]);
		}
	}
}


void TNDynamicAdiabaticPipeElement::setNodalConditions(double mdot, double TInlet, double TOutlet)
{
	// copy mass flux
	m_massFlux = mdot;

	// mass flux from inlet to outlet
	if(mdot >= 0) {
		// retrieve inlet specific enthalpy
		m_inletSpecificEnthalpy = TInlet * m_fluidHeatCapacity;
		// calculate inlet temperature
		m_inletTemperature = TInlet;
		// set outlet specific enthalpy
		m_outletSpecificEnthalpy = m_specificEnthalpies[m_nVolumes - 1];
		// set outlet temperature
		m_outletTemperature = m_temperatures[m_nVolumes - 1];
	}
	// reverse direction
	else {
		// retrieve outlet specific enthalpy
		m_outletSpecificEnthalpy = TOutlet * m_fluidHeatCapacity;
		// calculate inlet temperature
		m_outletTemperature = TOutlet;
		// set outlet specific enthalpy
		m_inletSpecificEnthalpy = m_specificEnthalpies[0];
		// set outlet temperature
		m_inletTemperature = m_temperatures[0];
	}
}


void TNDynamicAdiabaticPipeElement::dependencies(const double *ydot, const double *y,
										const double *mdot, const double* TInlet, const double*TOutlet,
										const double */*Qdot*/,
										std::vector<std::pair<const double *, const double *> > & resultInputDependencies) const {

	// set dependency to inlet and outlet enthalpy
	resultInputDependencies.push_back(std::make_pair(TInlet, y) );
	resultInputDependencies.push_back(std::make_pair(TOutlet, y + nInternalStates() - 1) );
	resultInputDependencies.push_back(std::make_pair(ydot, TInlet) );
	resultInputDependencies.push_back(std::make_pair(ydot + nInternalStates() - 1, TOutlet) );


	for(unsigned int n = 0; n < nInternalStates(); ++n) {

		// heat balance per default sums up heat fluxes and entahpy flux differences through the pipe
		if(n > 0)
			resultInputDependencies.push_back(std::make_pair(ydot + n, y + n - 1) );

		resultInputDependencies.push_back(std::make_pair(ydot + n, y + n) );

		if(n < nInternalStates() - 1)
			resultInputDependencies.push_back(std::make_pair(ydot + n, y + n + 1) );

		// set depedency to mdot
		resultInputDependencies.push_back(std::make_pair(ydot + n, mdot) );
	}
}

// *** HeatExchanger ***

TNHeatExchanger::TNHeatExchanger(const NANDRAD::HydraulicNetworkElement & elem,
							 const NANDRAD::HydraulicNetworkComponent & comp,
							const NANDRAD::HydraulicFluid & fluid)
{
	m_fluidVolume = comp.m_para[NANDRAD::HydraulicNetworkComponent::P_Volume].value;
//	m_UAValue = comp.m_para[NANDRAD::HydraulicNetworkComponent::P_UAValue].value;
	m_heatLoss = elem.m_para[NANDRAD::HydraulicNetworkElement::P_HeatFlux].value;
	// copy fluid properties
	m_fluidDensity = fluid.m_para[NANDRAD::HydraulicFluid::P_Density].value;
	m_fluidHeatCapacity = fluid.m_para[NANDRAD::HydraulicFluid::P_HeatCapacity].value;
}

TNHeatExchanger::~TNHeatExchanger()
{

}



// *** Pump ***

TNPump::TNPump(const NANDRAD::HydraulicNetworkElement & /*elem*/,
							 const NANDRAD::HydraulicNetworkComponent & comp,
							const NANDRAD::HydraulicFluid & fluid)
{
	m_fluidVolume = comp.m_para[NANDRAD::HydraulicNetworkComponent::P_Volume].value;
	// copy fluid properties
	m_fluidDensity = fluid.m_para[NANDRAD::HydraulicFluid::P_Density].value;
	m_fluidHeatCapacity = fluid.m_para[NANDRAD::HydraulicFluid::P_HeatCapacity].value;
}

TNPump::~TNPump()
{

}

} // namespace NANDRAD_MODEL
