/*	NANDRAD Solver Framework and Model Implementation.

	Copyright (c) 2012-today, Institut für Bauklimatik, TU Dresden, Germany

	Primary authors:
	  Andreas Nicolai  <andreas.nicolai -[at]- tu-dresden.de>
	  Anne Paepcke     <anne.paepcke -[at]- tu-dresden.de>

	This program is part of SIM-VICUS (https://github.com/ghorwin/SIM-VICUS)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
*/

#include "NM_HydraulicNetworkFlowElements.h"
#include "NM_Physics.h"

#include <NANDRAD_HydraulicNetworkElement.h>
#include <NANDRAD_HydraulicNetworkPipeProperties.h>
#include <NANDRAD_HydraulicNetworkComponent.h>
#include <NANDRAD_HydraulicNetworkControlElement.h>
#include <NANDRAD_HydraulicFluid.h>
#include <NANDRAD_Thermostat.h>

#include "NM_ThermalNetworkFlowElements.h"

#define PI				3.141592653589793238


namespace NANDRAD_MODEL {

// Definition of destructor is here, so that we have the code and virtual function table
// only once.
HydraulicNetworkAbstractFlowElement::~HydraulicNetworkAbstractFlowElement() {
}


// *** HNPipeElement ***

HNPipeElement::HNPipeElement(const NANDRAD::HydraulicNetworkElement & elem,
							const NANDRAD::HydraulicNetworkPipeProperties & pipePara,
							const NANDRAD::HydraulicFluid & fluid,
							const NANDRAD::HydraulicNetworkControlElement *controller,
							const std::vector<NANDRAD::Thermostat> & thermostats):
	m_fluid(&fluid),
	m_controlElement(controller),
	m_thermostats(thermostats)
{
	m_length = elem.m_para[NANDRAD::HydraulicNetworkElement::P_Length].value;
	m_diameter = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_PipeInnerDiameter].value;
	m_roughness = pipePara.m_para[NANDRAD::HydraulicNetworkPipeProperties::P_PipeRoughness].value;
	m_nParallelPipes = (unsigned int) elem.m_intPara[NANDRAD::HydraulicNetworkElement::IP_NumberParallelPipes].value;
}


void HNPipeElement::modelQuantities(std::vector<QuantityDescription> & quantities) const{
	if(m_controlElement == nullptr)
		return;
	// calculate zetaControlled value for valve
	quantities.push_back(QuantityDescription("ControllerResultValue","---", "The calculated controller zeta value for the valve", false));
}

void HNPipeElement::modelQuantityValueRefs(std::vector<const double *> & valRefs) const {
	if(m_controlElement == nullptr)
		return;
	// calculate zetaControlled value for valve
	valRefs.push_back(&m_zetaControlled);
}


void HNPipeElement::inputReferences(std::vector<InputReference> & inputRefs) const
{
	// in the case of control add heat exchange spline value to input references
	if (m_controlElement != nullptr)
	{
		// loop over all models, pick out the Thermostat-models and request input for a single zone. Later we check
		// that one (and exactly) one request must be fulfilled!
		for (const NANDRAD::Thermostat &thermostat : m_thermostats ) {
			// create an input reference to heating and cooling control values for all the constructions that we heat
			InputReference inputRef;
			inputRef.m_id = thermostat.m_id;
			inputRef.m_referenceType = NANDRAD::ModelInputReference::MRT_MODEL;
			inputRef.m_name.m_name = "HeatingControlValue";
			inputRef.m_name.m_index = (int) m_controlElement->m_idReferences[NANDRAD::HydraulicNetworkControlElement::ID_ThermostatZoneId]; // vector gets ID of requested zone
			inputRef.m_required = false;
			inputRefs.push_back(inputRef);
			inputRef.m_name.m_name = "CoolingControlValue";
			inputRefs.push_back(inputRef);
		}
	}
}


void HNPipeElement::setInputValueRefs(std::vector<const double*>::const_iterator & resultValueRefs) {
	FUNCID(HNPipeElement::setInputValueRefs);

	// in the case of control add heat exchange spline value to input references
	if (m_controlElement == nullptr)
		return;

	// now store the pointer returned for our input ref request and advance the iterator by one
	// m_heatingThermostatValueRef and m_coolingThermostatValueRef are initially nullptr -> not set
	for (unsigned int i=0; i< m_thermostats.size(); ++i) {
		// heating control value
		if (*resultValueRefs != nullptr) {
			// we must not yet have a reference!
			if (m_heatingThermostatControlValueRef != nullptr)
				throw IBK::Exception(IBK::FormatString("Duplicate heating control value result generated by different thermostats "
													   "for zone id=%1.")
									 .arg(m_controlElement->m_idReferences[NANDRAD::HydraulicNetworkControlElement::ID_ThermostatZoneId]), FUNC_ID);
			m_heatingThermostatControlValueRef = *resultValueRefs;
		}
		++resultValueRefs;
		// cooling control value
		if (*resultValueRefs != nullptr) {
			// we must not yet have a reference!
			if( m_coolingThermostatControlValueRef != nullptr)
				throw IBK::Exception(IBK::FormatString("Duplicate cooling control value result generated by different thermostats "
													   "for zone id=%1.")
									 .arg(m_controlElement->m_idReferences[NANDRAD::HydraulicNetworkControlElement::ID_ThermostatZoneId]), FUNC_ID);
			m_coolingThermostatControlValueRef = *resultValueRefs;
		}
		++resultValueRefs;
	}
}


double  HNPipeElement::systemFunction(double mdot, double p_inlet, double p_outlet) const {
	// In case of multiple parallel pipes, mdot is the mass flux through *all* pipes
	// (but all parallel sections together need the same pressure drop as a single one
	return p_inlet - p_outlet - pressureLossFriction(mdot/m_nParallelPipes);	// this is the system function
}


void HNPipeElement::partials(double mdot, double p_inlet, double p_outlet,
							 double & df_dmdot, double & df_dp_inlet, double & df_dp_outlet) const
{
	// partial derivatives of the system function to pressures are constants
	df_dp_inlet = 1;
	df_dp_outlet = -1;

	// generic DQ approximation of partial derivative
	const double EPS = 1e-5; // in kg/s
	double f_eps = systemFunction(mdot+EPS, p_inlet, p_outlet);
	double f = systemFunction(mdot, p_inlet, p_outlet);
	df_dmdot = (f_eps - f)/EPS;
}


double HNPipeElement::pressureLossFriction(const double &mdot) const {
	// for negative mass flow: Reynolds number is positive, velocity and pressure loss are negative
	double fluidDensity = m_fluid->m_para[NANDRAD::HydraulicFluid::P_Density].value;
	double velocity = mdot / (fluidDensity * m_diameter * m_diameter * PI / 4);
	double Re = std::abs(velocity) * m_diameter / m_fluid->m_kinematicViscosity.m_values.value(*m_fluidTemperatureRef);
	double zeta = m_length / m_diameter * FrictionFactorSwamee(Re, m_diameter, m_roughness);

	// add controlled zeta
	if(m_controlElement != nullptr)
		zeta += zetaControlled();

	return zeta * fluidDensity / 2 * std::abs(velocity) * velocity;
}


double HNPipeElement::zetaControlled() const {
	// valve is closed by default
	double heatingControlValue = m_controlElement->m_maximumControllerResultValue;
	double coolingControlValue = m_controlElement->m_maximumControllerResultValue;

	// get control value for heating
	if (m_heatingThermostatControlValueRef != nullptr) {
		// het heating control value and clip to the range of 0..1
		// 0 - no heating required (above setpoint)
		// 1 - max. heating required (for p-controller, the defined allowed tolerance has been reached
		//     for example, if Thermostat tolerance in set to 0.1 K, and the temperature difference to
		//     the setpoint is >= 0.1 K, heatingControlValue will be 1
		heatingControlValue = std::min(std::max(*m_heatingThermostatControlValueRef, 0.0), 1.0);

		// if heating is required, we open the valve:
		//   heatingControlValue = 1  -> zetaControlled = 0
		// if no heating is required, we close the valve
		//   heatingControlValue = 0  -> zetaControlled = m_maximumControllerResultValue
		// in between we interpolate linearly
		heatingControlValue = m_controlElement->m_maximumControllerResultValue * (1.0 - heatingControlValue);
	}

	// get control value for cooling
	if (m_coolingThermostatControlValueRef != nullptr) {
		// same as for heating
		coolingControlValue = std::min(std::max(*m_coolingThermostatControlValueRef, 0.0), 1.0);
		coolingControlValue = m_controlElement->m_maximumControllerResultValue * (1.0 - coolingControlValue);
	}

	// either cooling or heating may require opening of the value
	// we take the lesser of the selected additional resistances
	return std::min(heatingControlValue, coolingControlValue);
}


void HNPipeElement::updateResults(double /*mdot*/, double /*p_inlet*/, double /*p_outlet*/) {
	// calculate zetaControlled value for valve
	if (m_controlElement != nullptr) {
		m_zetaControlled = zetaControlled();
	}
}



// *** HNFixedPressureLossCoeffElement ***

HNPressureLossCoeffElement::HNPressureLossCoeffElement(unsigned int flowElementId,
														const NANDRAD::HydraulicNetworkComponent &component,
														const NANDRAD::HydraulicFluid &fluid,
														const NANDRAD::HydraulicNetworkControlElement *controlElement):
	m_flowElementId(flowElementId),
	m_controlElement(controlElement)
{
	m_fluidDensity = fluid.m_para[NANDRAD::HydraulicFluid::P_Density].value;
	m_fluidHeatCapacity = fluid.m_para[NANDRAD::HydraulicFluid::P_HeatCapacity].value;
	m_zeta = component.m_para[NANDRAD::HydraulicNetworkComponent::P_PressureLossCoefficient].value;
	m_diameter = component.m_para[NANDRAD::HydraulicNetworkComponent::P_HydraulicDiameter].value;
}


void HNPressureLossCoeffElement::inputReferences(std::vector<InputReference> & inputRefs) const {
	if (m_controlElement == nullptr)
		return; 	// only handle input reference when there is a controller

	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference: {
			InputReference ref;
			ref.m_id = m_flowElementId;
			ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
			ref.m_name.m_name = "HeatExchangeHeatLoss";
			ref.m_required = true;
			inputRefs.push_back(ref);
		} break;

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement: {
			InputReference ref;
			ref.m_id = m_followingflowElementId;
			ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
			ref.m_name.m_name = "FluidTemperature";
			ref.m_required = true;
			inputRefs.push_back(ref);
		} break;

		default: ; // other control elements do not require inputs
	}
}


void HNPressureLossCoeffElement::setInputValueRefs(std::vector<const double*>::const_iterator & resultValueRefs) {
	if (m_controlElement == nullptr)
		return; 	// only handle input reference when there is a controller

	switch (m_controlElement->m_controlledProperty) {
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference :
			// now store the pointer returned for our input ref request and advance the iterator by one
			m_heatExchangeHeatLossRef = *(resultValueRefs++); // Heat exchange value reference
		break;

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement :
			// now store the pointer returned for our input ref request and advance the iterator by one
			m_followingFlowElementFluidTemperatureRef = *(resultValueRefs++); // Fluid temperature of following element
		break;

		default: ; // other control elements do not require inputs
	}
}


void HNPressureLossCoeffElement::modelQuantities(std::vector<QuantityDescription> & quantities) const{
	if (m_controlElement == nullptr)
		return;
	// calculate zetaControlled value for valve
	quantities.push_back(QuantityDescription("ControllerResultValue","---", "The calculated controller zeta value for the valve", false));
	if (m_controlElement->m_controlledProperty == NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference)
		quantities.push_back(QuantityDescription("TemperatureDifference","K", "The difference between inlet and outlet temperature", false));
}

void HNPressureLossCoeffElement::modelQuantityValueRefs(std::vector<const double *> & valRefs) const {
	if (m_controlElement == nullptr)
		return;
	// calculate zetaControlled value for valve
	valRefs.push_back(&m_zetaControlled);
	if (m_controlElement->m_controlledProperty == NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference)
		valRefs.push_back(&m_temperatureDifference);
}



double HNPressureLossCoeffElement::systemFunction(double mdot, double p_inlet, double p_outlet) const {
	// for negative mass flow: dp is negative
	double area = PI / 4 * m_diameter * m_diameter;
	double velocity = mdot / (m_fluidDensity * area); // signed!
	double zeta = m_zeta;
	if (m_controlElement != nullptr) {
		zeta += zetaControlled(mdot); // no clipping necessary here, function zetaControlled() takes care of that!
	}
	double dp = zeta * m_fluidDensity / 2 * std::abs(velocity) * velocity;
	return p_inlet - p_outlet - dp;
}


double HNPressureLossCoeffElement::zetaControlled(double mdot) const {
	FUNCID(TNElementWithExternalHeatLoss::zetaControlled);

	// calculate zetaControlled value for valve
	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference:
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement: {

			double temperatureDifference;
			// -> CP_TemperatureDifference
			if (m_controlElement->m_controlledProperty == NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference){
				IBK_ASSERT(m_heatExchangeHeatLossRef != nullptr);
				// compute current temperature for given heat loss and mass flux
				// Mind: access m_heatExchangeValueRef and not m_heatLoss here!
				temperatureDifference = *m_heatExchangeHeatLossRef/(mdot*m_fluidHeatCapacity);
			}
			// -> CP_TemperatureDifferenceOfFollowingElement
			else {
				// compute temperature difference of the following element. We already know that the node between this
				// and the following element is not connected to any other flow element
				IBK_ASSERT(m_followingFlowElementFluidTemperatureRef != nullptr);
				temperatureDifference = (*m_fluidTemperatureRef - *m_followingFlowElementFluidTemperatureRef);
			}

			// if temperature difference is larger than the set point (negative e), we want maximum mass flux -> zeta = 0
			// if temperature difference is smaller than the set point (positive e), we decrease mass flow by increasing zeta
			const double e = m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_TemperatureDifferenceSetpoint].value - temperatureDifference;

			// TODO : use controller object here
			double zetaControlled = 0.0;
			if (e <= 0) {
				zetaControlled = 0;
			}
			else {
				switch (m_controlElement->m_controllerType) {
					case NANDRAD::HydraulicNetworkControlElement::CT_PController: {
						// relate controller error e to zeta
						const double y = m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_Kp].value * e;
						const double zetaMax = m_controlElement->m_maximumControllerResultValue;
						// apply clipping
						if (zetaMax > 0 && y > zetaMax)
							zetaControlled = zetaMax; // Note: this is problematic inside a Newton method without relaxation!
						else {
							zetaControlled = y;
						}
					} break;

					case NANDRAD::HydraulicNetworkControlElement::CT_PIController:
						throw IBK::Exception("PIController not implemented, yet.", FUNC_ID);

					case NANDRAD::HydraulicNetworkControlElement::NUM_CT: break; // just to make compiler happy
				}

			}
			return zetaControlled;
		}


		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: {
			// e is > 0 if our mass flux exceeds the limit
			const double e = mdot - m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_MassFluxSetpoint].value;

			// TODO : use controller object here
			double zetaControlled = 0.0;
			if (e <= 0) {
				zetaControlled = 0;
			}
			else {
				switch (m_controlElement->m_controllerType) {
					case NANDRAD::HydraulicNetworkControlElement::CT_PController: {
						// relate controller error e to zeta
						const double y = m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_Kp].value * e;
						const double zetaMax = m_controlElement->m_maximumControllerResultValue;
						// apply clipping
						if (zetaMax > 0 && y > zetaMax)
							zetaControlled = zetaMax; // Note: this is problematic inside a Newton method without relaxation!
						else {
							zetaControlled = y;
						}
					} break;

					case NANDRAD::HydraulicNetworkControlElement::CT_PIController:
						throw IBK::Exception("PIController not implemented, yet.", FUNC_ID);

					case NANDRAD::HydraulicNetworkControlElement::NUM_CT: break; // just to make compiler happy
				}

			}
			return zetaControlled;
		}

		case NANDRAD::HydraulicNetworkControlElement::CP_ThermostatValue: // not a possible combination
		case NANDRAD::HydraulicNetworkControlElement::NUM_CP: ; // nothing todo - we return 0
	}
//	IBK::IBK_Message(IBK::FormatString("zeta = %1, m_heatLoss = %4 W, dT = %2 K, mdot = %3 kg/s, heatExchangeValueRef = %5 W\n")
//					 .arg(m_zetaControlled).arg(m_temperatureDifference).arg(mdot).arg(m_heatLoss).arg(*m_heatExchangeValueRef));
	return 0.0;
}



void HNPressureLossCoeffElement::partials(double mdot, double p_inlet, double p_outlet,
							 double & df_dmdot, double & df_dp_inlet, double & df_dp_outlet) const
{
	// partial derivatives of the system function to pressures are constants
	df_dp_inlet = 1;
	df_dp_outlet = -1;
	// generic DQ approximation of partial derivative
	const double EPS = 1e-5; // in kg/s
	double f_eps = systemFunction(mdot+EPS, p_inlet, p_outlet);
	double f = systemFunction(mdot, p_inlet, p_outlet);
	df_dmdot = (f_eps - f)/EPS;
}


void HNPressureLossCoeffElement::updateResults(double mdot, double /*p_inlet*/, double /*p_outlet*/) {
	if (m_controlElement == nullptr)
		return;

	// calculate zetaControlled value for valve
	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference: {
			IBK_ASSERT(m_heatExchangeHeatLossRef != nullptr);
			// compute current temperature for given heat loss and mass flux
			// Mind: access m_heatExchangeValueRef and not m_heatLoss here!
			m_temperatureDifference = *m_heatExchangeHeatLossRef/(mdot*m_fluidHeatCapacity);
			m_zetaControlled = zetaControlled(mdot);
		} break;

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement: {
			IBK_ASSERT(m_followingFlowElementFluidTemperatureRef != nullptr);
			// compute current temperature difference of following element
			m_temperatureDifference = (*m_fluidTemperatureRef - *m_followingFlowElementFluidTemperatureRef);
			m_zetaControlled = zetaControlled(mdot);
		} break;

		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: // not a possible combination
			m_zetaControlled = zetaControlled(mdot);
		break;

		case NANDRAD::HydraulicNetworkControlElement::CP_ThermostatValue: // not a possible combination
		case NANDRAD::HydraulicNetworkControlElement::NUM_CP: ; // nothing todo - we return 0
	}
}


// *** HNConstantPressurePump ***

HNConstantPressurePump::HNConstantPressurePump(unsigned int id, const NANDRAD::HydraulicNetworkComponent &component) :
	m_id(id)
{
	m_pressureHead = component.m_para[NANDRAD::HydraulicNetworkComponent::P_PressureHead].value;
}


double HNConstantPressurePump::systemFunction(double /*mdot*/, double p_inlet, double p_outlet) const {
	if (m_pressureHeadRef != nullptr)
		return p_inlet - p_outlet + *m_pressureHeadRef;
	else
		return p_inlet - p_outlet + m_pressureHead;
}


void HNConstantPressurePump::partials(double /*mdot*/, double /*p_inlet*/, double /*p_outlet*/,
							 double & df_dmdot, double & df_dp_inlet, double & df_dp_outlet) const
{
	// partial derivatives of the system function to pressures are constants
	df_dp_inlet = 1;
	df_dp_outlet = -1;
	df_dmdot = 0;
}


void HNConstantPressurePump::inputReferences(std::vector<InputReference> & inputRefs) const {
	InputReference inputRef;
	inputRef.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
	inputRef.m_name = std::string("PumpPressureHead");
	inputRef.m_required = false;
	inputRef.m_id = m_id;
	inputRefs.push_back(inputRef);
}


void HNConstantPressurePump::setInputValueRefs(std::vector<const double *>::const_iterator & resultValueRefIt) {
	m_pressureHeadRef = *resultValueRefIt; // optional, may be nullptr
	++resultValueRefIt;
}


// *** HNControlledPump ***

HNControlledPump::HNControlledPump(unsigned int id,
												   unsigned int followingflowElementId,
												   const NANDRAD::HydraulicNetworkControlElement *controlElement) :
	m_id(id),
	m_followingflowElementId(followingflowElementId),
	m_controlElement(controlElement)
{
	if(m_controlElement->m_controllerType == NANDRAD::HydraulicNetworkControlElement::CP_MassFlux)
		m_setpointMassFluxRef = &m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_MassFluxSetpoint].value;
}


void HNControlledPump::modelQuantities(std::vector<QuantityDescription> &quantities) const {
	if(m_controlElement == nullptr)
		return;
	// calculate zetaControlled value for valve
	quantities.push_back(QuantityDescription("PumpPressureHead","Pa", "The calculated controlled pressure head of the pump", false));
}


void HNControlledPump::modelQuantityValueRefs(std::vector<const double*> &valRefs) const {
	valRefs.push_back(&m_pressureHead);
}


void HNControlledPump::inputReferences(std::vector<InputReference> & inputRefs) const {
	if (m_controlElement == nullptr)
		return; 	// only handle input reference when there is a controller

	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement: {
			InputReference ref;
			ref.m_id = m_followingflowElementId;
			ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
			ref.m_name.m_name = "FluidTemperature";
			ref.m_required = true;
			inputRefs.push_back(ref);
		} break;
		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: {
			InputReference ref;
			ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
			ref.m_name = std::string("SetpointMassFlux");
			ref.m_required = false;
			ref.m_id = m_id;
			inputRefs.push_back(ref);
		} break;

		default: ; // other control elements do not require inputs
	}
}


void HNControlledPump::setInputValueRefs(std::vector<const double*>::const_iterator & resultValueRefs) {
	if (m_controlElement == nullptr)
		return; 	// only handle input reference when there is a controller

	switch (m_controlElement->m_controlledProperty) {
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement :
			// now store the pointer returned for our input ref request and advance the iterator by one
			m_followingFlowElementFluidTemperatureRef = *(resultValueRefs++); // Fluid temperature of following element
		break;
		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux:
			// setpoint mass flux may be zero
			if(*resultValueRefs != nullptr)
				m_setpointMassFluxRef = *(resultValueRefs++); // Fluid temperature of following element
		break;
		default: ; // other control elements do not require inputs
	}
}


double HNControlledPump::systemFunction(double mdot, double p_inlet, double p_outlet) const {
	if (m_controlElement != nullptr)
		return p_inlet - p_outlet + pressureHeadControlled(mdot);
	else
		return p_inlet - p_outlet;
}


void HNControlledPump::partials(double mdot, double p_inlet, double p_outlet,
							 double & df_dmdot, double & df_dp_inlet, double & df_dp_outlet) const
{
	df_dp_inlet = 1;
	df_dp_outlet = -1;
	// partial derivatives of the system function to pressures are constants
	if(m_controlElement == nullptr ) {
		df_dmdot = 0;
	}
	else {
		// generic DQ approximation of partial derivative
		const double EPS = 1e-5; // in kg/s
		double f_eps = systemFunction(mdot+EPS, p_inlet, p_outlet);
		double f = systemFunction(mdot, p_inlet, p_outlet);
		df_dmdot = (f_eps - f)/EPS;
	}
}


double HNControlledPump::pressureHeadControlled(double mdot) const {
	FUNCID(HNControlledMassFluxPump::pressureHeadControlled);

	// calculate zetaControlled value for valve
	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement: {
			double temperatureDifference;
			// compute temperature difference of the following element. We already know that the node between this
			// and the following element is not connected to any other flow element
			IBK_ASSERT(m_followingFlowElementFluidTemperatureRef != nullptr);
			temperatureDifference = (*m_fluidTemperatureRef - *m_followingFlowElementFluidTemperatureRef);

			// if temperature difference is larger than the set point (negative e), we want maximum mass flux -> zeta = 0
			// if temperature difference is smaller than the set point (positive e), we decrease mass flow by increasing zeta
			const double e = temperatureDifference - m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_TemperatureDifferenceSetpoint].value;

			// TODO : use controller object here
			double pressHeadControlled = 0.0;
			if (e <= 0) {
				pressHeadControlled = 0;
			}
			else {
				switch (m_controlElement->m_controllerType) {
					case NANDRAD::HydraulicNetworkControlElement::CT_PController: {
						// relate controller error e to zeta
						const double y = m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_Kp].value * e;
						const double pressHeadMax = m_controlElement->m_maximumControllerResultValue;
						// apply clipping
						if (pressHeadMax > 0 && y > pressHeadMax)
							pressHeadControlled = pressHeadMax; // Note: this is problematic inside a Newton method without relaxation!
						else {
							pressHeadControlled = y;
						}
					} break;

					case NANDRAD::HydraulicNetworkControlElement::CT_PIController:
						throw IBK::Exception("PIController not implemented, yet.", FUNC_ID);

					case NANDRAD::HydraulicNetworkControlElement::NUM_CT: break; // just to make compiler happy
				}

			}
			return pressHeadControlled;
		}
		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: {
			// external reference or constant parameter
			const double mdotSetpoint = *m_setpointMassFluxRef;

			// setpoint 0 returns 0
			if(mdotSetpoint == 0.0)
				return 0.0;
			// e is > 0 if our mass flux exceeds the limit
			const double e = mdotSetpoint - mdot;

			// TODO : use controller object here
			double pressHeadControlled = 0.0;
			if (e <= 0) {
				pressHeadControlled = 0;
			}
			else {
				switch (m_controlElement->m_controllerType) {
					case NANDRAD::HydraulicNetworkControlElement::CT_PController: {
						// relate controller error e to zeta
						const double pressHeadMax = m_controlElement->m_maximumControllerResultValue;
						const double y = pressHeadMax/mdotSetpoint * e;
						// apply clipping
						if (pressHeadMax > 0 && y > pressHeadMax)
							pressHeadControlled = pressHeadMax; // Note: this is problematic inside a Newton method without relaxation!
						else {
							pressHeadControlled = y;
						}
					} break;

					case NANDRAD::HydraulicNetworkControlElement::CT_PIController:
						throw IBK::Exception("PIController not implemented, yet.", FUNC_ID);

					case NANDRAD::HydraulicNetworkControlElement::NUM_CT: break; // just to make compiler happy
				}

			}
			return pressHeadControlled;
		}

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference:
		case NANDRAD::HydraulicNetworkControlElement::CP_ThermostatValue: // not a possible combination
		case NANDRAD::HydraulicNetworkControlElement::NUM_CP: ; // nothing todo - we return 0
	}
//	IBK::IBK_Message(IBK::FormatString("zeta = %1, m_heatLoss = %4 W, dT = %2 K, mdot = %3 kg/s, heatExchangeValueRef = %5 W\n")
//					 .arg(m_zetaControlled).arg(m_temperatureDifference).arg(mdot).arg(m_heatLoss).arg(*m_heatExchangeValueRef));
	return 0.0;
}

void HNControlledPump::updateResults(double mdot, double /*p_inlet*/, double /*p_outlet*/) {
	if (m_controlElement == nullptr)
		return;

	// calculate zetaControlled value for valve
	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement:
		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: // not a possible combination
			m_pressureHead = pressureHeadControlled(mdot);
		break;

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference:
		case NANDRAD::HydraulicNetworkControlElement::CP_ThermostatValue: // not a possible combination
		case NANDRAD::HydraulicNetworkControlElement::NUM_CP: ; // nothing todo - we return 0
	}
}



} // namespace NANDRAD_MODEL
