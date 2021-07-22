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


void HNPipeElement::inputReferences(std::vector<InputReference> & inputRefs) const {
	// in the case of control add heat exchange spline value to input references
	if (m_controlElement != nullptr) {
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


void HNPipeElement::dependencies(const double * mdot, std::vector<std::pair<const double *, const double *> > & resultInputDependencies) const {
	// here we define dependencies between mdot and controller-inputs
	if (m_heatingThermostatControlValueRef != nullptr)
		resultInputDependencies.push_back(std::make_pair(mdot, m_heatingThermostatControlValueRef) );
	if (m_coolingThermostatControlValueRef != nullptr)
		resultInputDependencies.push_back(std::make_pair(mdot, m_coolingThermostatControlValueRef) );
}


double  HNPipeElement::systemFunction(double mdot, double p_inlet, double p_outlet) const {
#if 0
	// test implementation of special bypass-pipe model
	if (m_diameter > 0.0049 && m_diameter < 0.0051) {
		const double MDOT_GEGEBEN = 0.02;
		double zeta = MDOT_GEGEBEN/(mdot+1e-10);
		zeta *= zeta;
		zeta *= zeta;
		zeta *= 100000000;
		double deltaP = zeta * std::abs(mdot) * mdot;
		const double DP_MAX = 2750.0;
		deltaP = std::min(deltaP, DP_MAX);
		return p_inlet - p_outlet - deltaP;
	}
#endif
	// In case of multiple parallel pipes, mdot is the mass flux through *all* pipes
	// (but all parallel sections together need the same pressure drop as a single one
	double deltaP = pressureLossFriction(mdot/m_nParallelPipes);
	return p_inlet - p_outlet - deltaP;	// this is the system function
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
	double velocity = mdot / (fluidDensity * m_diameter * m_diameter * PI / 4.0);
	double Re = std::abs(velocity) * m_diameter / m_fluid->m_kinematicViscosity.m_values.value(*m_fluidTemperatureRef);
	double zeta = m_length / m_diameter * FrictionFactorSwamee(Re, m_diameter, m_roughness);

	// add controlled zeta
	if (m_controlElement != nullptr)
		zeta += zetaControlled();

	return zeta * fluidDensity / 2.0 * std::abs(velocity) * velocity;
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
		double e = (1.0 - heatingControlValue);
		heatingControlValue = m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_Kp].value * e; // std::pow(10.,-2*(1-e));
		// clip against max value
		heatingControlValue = std::min(heatingControlValue, m_controlElement->m_maximumControllerResultValue);
	}

	// get control value for cooling
	if (m_coolingThermostatControlValueRef != nullptr) {
		// TODO : same as for heating
		coolingControlValue = std::min(std::max(*m_coolingThermostatControlValueRef, 0.0), 1.0);
		double e = (1.0 - coolingControlValue);
		coolingControlValue = m_controlElement->m_maximumControllerResultValue * e;
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



// *** HNPressureLossCoeffElement ***

HNPressureLossCoeffElement::HNPressureLossCoeffElement(unsigned int flowElementId,
														const NANDRAD::HydraulicNetworkComponent &component,
														const NANDRAD::HydraulicFluid &fluid,
														const NANDRAD::HydraulicNetworkControlElement *controlElement):
	m_id(flowElementId),
	m_controlElement(controlElement)
{
	m_fluidDensity = fluid.m_para[NANDRAD::HydraulicFluid::P_Density].value;
	m_fluidHeatCapacity = fluid.m_para[NANDRAD::HydraulicFluid::P_HeatCapacity].value;
	m_zeta = component.m_para[NANDRAD::HydraulicNetworkComponent::P_PressureLossCoefficient].value;
	m_diameter = component.m_para[NANDRAD::HydraulicNetworkComponent::P_HydraulicDiameter].value;

	// initialize set point pointers, in case of scheduled parameters the pointers will be updated in setInputValueRefs()
	m_temperatureDifferenceSetpointRef = &m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_TemperatureDifferenceSetpoint].value;
	m_massFluxSetpointRef = &m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_MassFluxSetpoint].value;
}


void HNPressureLossCoeffElement::inputReferences(std::vector<InputReference> & inputRefs) const {
	if (m_controlElement == nullptr)
		return; 	// only handle input reference when there is a controller

	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference: {
			InputReference ref;
			ref.m_id = m_id;
			ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
			ref.m_name.m_name = "HeatExchangeHeatLoss";
			ref.m_required = true;
			inputRefs.push_back(ref);
			// if we have a scheduled temperature difference setpoint, also generate input reference for
			// the scheduled value
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled) {
				InputReference ref;
				ref.m_id = m_id;
				ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
				ref.m_name.m_name = "TemperatureDifferenceSetpointSchedule";
				ref.m_required = true;
				inputRefs.push_back(ref);
			}
		} break;

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement: {
			InputReference ref;
			ref.m_id = m_followingflowElementId;
			ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
			ref.m_name.m_name = "FluidTemperature";
			ref.m_required = true;
			inputRefs.push_back(ref);
			// if we have a scheduled temperature difference setpoint, also generate input reference for
			// the scheduled value
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled) {
				InputReference ref;
				ref.m_id = m_id;
				ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
				ref.m_name.m_name = "TemperatureDifferenceSetpointSchedule";
				ref.m_required = true;
				inputRefs.push_back(ref);
			}
		} break;

		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: {
			// only create input reference for scheduled variant
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled) {
				InputReference ref;
				ref.m_id = m_id;
				ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
				ref.m_name.m_name = "MassFluxSetpointSchedule";
				ref.m_required = true;
				inputRefs.push_back(ref);
			}
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
			// scheduled variant?
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled)
				m_temperatureDifferenceSetpointRef = *(resultValueRefs++);
		break;

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement :
			// now store the pointer returned for our input ref request and advance the iterator by one
			m_followingFlowElementFluidTemperatureRef = *(resultValueRefs++); // Fluid temperature of following element
			// scheduled variant?
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled)
				m_temperatureDifferenceSetpointRef = *(resultValueRefs++);
		break;

		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: {
			// scheduled variant?
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled)
				m_massFluxSetpointRef = *(resultValueRefs++);
		} break;
		default: ; // other control elements do not require inputs
	}
}


void HNPressureLossCoeffElement::modelQuantities(std::vector<QuantityDescription> & quantities) const{
	if (m_controlElement == nullptr)
		return;
	// calculate zetaControlled value for valve
	quantities.push_back(QuantityDescription("ControllerResultValue","---", "The calculated controller zeta value for the valve", false));
}


void HNPressureLossCoeffElement::modelQuantityValueRefs(std::vector<const double *> & valRefs) const {
	if (m_controlElement == nullptr)
		return;
	// calculate zetaControlled value for valve
	valRefs.push_back(&m_zetaControlled);
}



double HNPressureLossCoeffElement::systemFunction(double mdot, double p_inlet, double p_outlet) const {
	// for negative mass flow: dp is negative
	double area = PI / 4 * m_diameter * m_diameter;
	double velocity = mdot / (m_fluidDensity * area); // signed!
	double zeta = m_zeta;
	if (m_controlElement != nullptr)
		zeta += zetaControlled(mdot); // no clipping necessary here, function zetaControlled() takes care of that!

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
			if (m_controlElement->m_controlledProperty == NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference) {
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
			const double e = *m_temperatureDifferenceSetpointRef - temperatureDifference;

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
			// e is > 0 if our mass flux exceeds the limit -> then we have to increase the flow resistance
			const double e = mdot - *m_massFluxSetpointRef;

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

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference:
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement:
		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux:
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
	// initialize value reference to pressure head, pointer will be updated for given schedules in setInputValueRefs()
	m_pressureHeadRef = &component.m_para[NANDRAD::HydraulicNetworkComponent::P_PressureHead].value;
}


double HNConstantPressurePump::systemFunction(double /*mdot*/, double p_inlet, double p_outlet) const {
	return p_inlet - p_outlet + *m_pressureHeadRef;
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
	// Note: this is an automatic override and could lead to problems. However, it is explicitely documented and
	//       a warning is added about this in the model description.
	InputReference inputRef;
	inputRef.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
	inputRef.m_name = std::string("PressureHeadSchedule");
	inputRef.m_required = false;
	inputRef.m_id = m_id;
	inputRefs.push_back(inputRef);
}


void HNConstantPressurePump::setInputValueRefs(std::vector<const double *>::const_iterator & resultValueRefIt) {
	if (*resultValueRefIt != nullptr)
		m_pressureHeadRef = *resultValueRefIt; // optional, may be nullptr
	++resultValueRefIt;
}



// *** HNConstantPressureLossValve ***

HNConstantPressureLossValve::HNConstantPressureLossValve(unsigned int id, const NANDRAD::HydraulicNetworkComponent &component) :
	m_id(id)
{
	m_pressureLoss = component.m_para[NANDRAD::HydraulicNetworkComponent::P_PressureLoss].value;
}


double HNConstantPressureLossValve::systemFunction(double mdot, double p_inlet, double p_outlet) const {
	if (mdot >= 0)
		return p_inlet - p_outlet - m_pressureLoss;
	else
		return p_inlet - p_outlet + m_pressureLoss;
}


void HNConstantPressureLossValve::partials(double /*mdot*/, double /*p_inlet*/, double /*p_outlet*/,
										   double & df_dmdot, double & df_dp_inlet, double & df_dp_outlet) const
{
	// partial derivatives of the system function to pressures are constants
	df_dp_inlet = 1;
	df_dp_outlet = -1;
	df_dmdot = 0;
}



// *** HNConstantMassFluxPump ***

HNConstantMassFluxPump::HNConstantMassFluxPump(unsigned int id, const NANDRAD::HydraulicNetworkComponent & component):
	m_id(id)
{
	// initialize mass flux
	m_massFluxRef = &component.m_para[NANDRAD::HydraulicNetworkComponent::P_MassFlux].value;
}


void HNConstantMassFluxPump::modelQuantities(std::vector<QuantityDescription> & quantities) const {
	quantities.push_back(QuantityDescription("PumpPressureHead","Pa", "The calculated controlled pressure head of the pump", false));
}


void HNConstantMassFluxPump::modelQuantityValueRefs(std::vector<const double *> & valRefs) const {
	valRefs.push_back(&m_pressureHead);
}


void HNConstantMassFluxPump::inputReferences(std::vector<InputReference> &inputRefs) const {
	InputReference ref;
	ref.m_id = m_id;
	ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
	ref.m_name.m_name = "MassFluxSchedule";
	ref.m_required = false;
	inputRefs.push_back(ref);
}


void HNConstantMassFluxPump::setInputValueRefs(std::vector<const double *>::const_iterator & resultValueRefIt) {
	// overwrite scheduled mass flux
	if(*resultValueRefIt != nullptr)
		m_massFluxRef = *resultValueRefIt;
	++resultValueRefIt;
}


double HNConstantMassFluxPump::systemFunction(double mdot, double /*p_inlet*/, double /*p_outlet*/) const {
	return (mdot - *m_massFluxRef);
}


void HNConstantMassFluxPump::partials(double /*mdot*/, double /*p_inlet*/, double /*p_outlet*/,
									  double & df_dmdot, double & df_dp_inlet, double & df_dp_outlet) const
{
	df_dmdot = 1;
	df_dp_inlet = 0.0;
	df_dp_outlet = 0.0;
}


void HNConstantMassFluxPump::updateResults(double /*mdot*/, double p_inlet, double p_outlet) {
	m_pressureHead = p_outlet - p_inlet;
}



// *** HNControlledPump ***

HNControlledPump::HNControlledPump(unsigned int id, const NANDRAD::HydraulicNetworkControlElement *controlElement) :
	m_controlElement(controlElement),
	m_id(id)
{
	// initialize setpoint references, in case of scheduled setpoints pointer will be updated in setInputValueRefs()
	m_temperatureDifferenceSetpointRef = &m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_TemperatureDifferenceSetpoint].value;
	m_massFluxSetpointRef = &m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_MassFluxSetpoint].value;
}


void HNControlledPump::modelQuantities(std::vector<QuantityDescription> &quantities) const {
	if (m_controlElement == nullptr)
		return;
	quantities.push_back(QuantityDescription("PumpPressureHead","Pa", "The calculated controlled pressure head of the pump", false));
	if (m_controlElement->m_controlledProperty == NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement)
		quantities.push_back(QuantityDescription("TemperatureDifference","K", "The difference between inlet and outlet temperature", false));
}


void HNControlledPump::modelQuantityValueRefs(std::vector<const double*> &valRefs) const {
	valRefs.push_back(&m_pressureHead);
	if (m_controlElement->m_controlledProperty == NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement)
		valRefs.push_back(&m_temperatureDifference);
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
			// if we have a scheduled temperature difference setpoint, also generate input reference for
			// the scheduled value
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled) {
				InputReference ref;
				ref.m_id = m_id;
				ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
				ref.m_name.m_name = "TemperatureDifferenceSetpointSchedule";
				ref.m_required = true;
				inputRefs.push_back(ref);
			}
		} break;

		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: {
			// only create input reference for scheduled variant
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled) {
				InputReference ref;
				ref.m_id = m_id;
				ref.m_referenceType = NANDRAD::ModelInputReference::MRT_NETWORKELEMENT;
				ref.m_name.m_name = "MassFluxSetpointSchedule";
				ref.m_required = true;
				inputRefs.push_back(ref);
			}
		} break;

		// other combinations are not supported
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference:
		case NANDRAD::HydraulicNetworkControlElement::CP_ThermostatValue:
		case NANDRAD::HydraulicNetworkControlElement::NUM_CP:
		break;
	}
}


void HNControlledPump::setInputValueRefs(std::vector<const double*>::const_iterator & resultValueRefs) {
	if (m_controlElement == nullptr)
		return; 	// only handle input reference when there is a controller

	switch (m_controlElement->m_controlledProperty) {
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement :
			// now store the pointer returned for our input ref request and advance the iterator by one
			m_followingFlowElementFluidTemperatureRef = *(resultValueRefs++); // Fluid temperature of following element
			// scheduled variant?
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled)
				m_temperatureDifferenceSetpointRef = *(resultValueRefs++);
		break;
		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux:
			// scheduled variant?
			if (m_controlElement->m_modelType == NANDRAD::HydraulicNetworkControlElement::MT_Scheduled)
				m_massFluxSetpointRef = *(resultValueRefs++);
		break;
		default: ; // other control elements do not require inputs
	}
}


void HNControlledPump::dependencies(const double * mdot, std::vector<std::pair<const double *, const double *> > & resultInputDependencies) const {
	if (m_controlElement == nullptr)
		return; // no control, no dependencies
	switch (m_controlElement->m_controlledProperty) {
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement:
			resultInputDependencies.push_back(std::make_pair(mdot, m_fluidTemperatureRef) );
			resultInputDependencies.push_back(std::make_pair(mdot, m_followingFlowElementFluidTemperatureRef) );
		break;
		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux:
			resultInputDependencies.push_back(std::make_pair(mdot, m_massFluxSetpointRef) );
		break;

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference:
		case NANDRAD::HydraulicNetworkControlElement::CP_ThermostatValue:
		case NANDRAD::HydraulicNetworkControlElement::NUM_CP: ;
	}
}


double HNControlledPump::systemFunction(double mdot, double p_inlet, double p_outlet) const {
	double deltaP = p_inlet - p_outlet;
	double pressHead = pressureHeadControlled(mdot);
	return deltaP + pressHead;
}


void HNControlledPump::partials(double mdot, double p_inlet, double p_outlet,
							 double & df_dmdot, double & df_dp_inlet, double & df_dp_outlet) const
{
	df_dp_inlet = 1;
	df_dp_outlet = -1;
	// partial derivatives of the system function to pressures are constants
	if (m_controlElement == nullptr ) {
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

	double e = 0;	// deviation of controlled property

	// calculate zetaControlled value for valve
	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement: {
			double temperatureDifference;
			// compute temperature difference of the following element. We already know that the node between this
			// and the following element is not connected to any other flow element
			temperatureDifference = (*m_fluidTemperatureRef - *m_followingFlowElementFluidTemperatureRef);

			// compute control error
			e = temperatureDifference - *m_temperatureDifferenceSetpointRef;
			// if temperature difference is smaller than the required difference (negative e), our mass flux is too large,
			// we stop it by returning 0
			//    -> pressHeadControlled = 0
			// if temperature difference is larger than the required difference (positive e), we gradually increase
			// mass flow by increasing pressure head of pump
			//    -> pressHeadControlled = e*Kp
		} break;


		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux: {
			// external reference or constant parameter
			const double mdotSetpoint = *m_massFluxSetpointRef;

			// setpoint 0 returns 0
			if (mdotSetpoint == 0.0)
				return 0.0;
			// compute controller error
			e = mdotSetpoint - mdot;
			// if e is > 0 if our mass flux is below the limit (we need to increase mass flux by increasing pressure head)
			// if e <= 0, our mass flux is too large, we turn it off
		} break;

		// not possible combinations
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference:
		case NANDRAD::HydraulicNetworkControlElement::CP_ThermostatValue:
		case NANDRAD::HydraulicNetworkControlElement::NUM_CP: ;
	}

	// TODO : use controller object here

	// if controller deviation is <= 0,  return 0
//	if (e <= 0) {
//		return 0;
//	}

	double pressHeadControlled = 0;
	double pressHeadMax = m_controlElement->m_maximumControllerResultValue;

	switch (m_controlElement->m_controllerType) {
		case NANDRAD::HydraulicNetworkControlElement::CT_PController: {
			// relate controller error e to zeta
			const double y = m_controlElement->m_para[NANDRAD::HydraulicNetworkControlElement::P_Kp].value * e;

			// apply clipping (only when pressureHeadMax > 0)
			if (y > pressHeadMax && pressHeadMax > 0)
				pressHeadControlled = pressHeadMax;
			else
				pressHeadControlled = y;
		} break;

		case NANDRAD::HydraulicNetworkControlElement::CT_PIController:
			throw IBK::Exception("PIController not implemented, yet.", FUNC_ID);

		case NANDRAD::HydraulicNetworkControlElement::NUM_CT: break; // just to make compiler happy
	}


	return pressHeadControlled;
}


void HNControlledPump::updateResults(double mdot, double /*p_inlet*/, double /*p_outlet*/) {
	if (m_controlElement == nullptr)
		return;

	// calculate zetaControlled value for valve
	switch (m_controlElement->m_controlledProperty) {

		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifferenceOfFollowingElement:
			m_temperatureDifference = (*m_fluidTemperatureRef - *m_followingFlowElementFluidTemperatureRef);
		break;

		case NANDRAD::HydraulicNetworkControlElement::CP_MassFlux:
			m_pressureHead = pressureHeadControlled(mdot);
		break;


		// not possible combinations
		case NANDRAD::HydraulicNetworkControlElement::CP_TemperatureDifference:
		case NANDRAD::HydraulicNetworkControlElement::CP_ThermostatValue:
		case NANDRAD::HydraulicNetworkControlElement::NUM_CP: ;
	}
}



} // namespace NANDRAD_MODEL
