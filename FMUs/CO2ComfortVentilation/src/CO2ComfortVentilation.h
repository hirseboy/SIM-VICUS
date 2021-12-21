/*

FMI Interface for FMU generated by FMICodeGenerator.

This file is part of FMICodeGenerator (https://github.com/ghorwin/FMICodeGenerator)

BSD 3-Clause License

Copyright (c) 2018, Andreas Nicolai
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef CO2ComfortVentilationH
#define CO2ComfortVentilationH

#include "fmi2common/InstanceData.h"

#include "LinearSpline.h"
#include "Path.h"

/*! This class wraps all data needed for a single instance of the FMU. */
class CO2ComfortVentilation : public InstanceData {
public:
	/*! Initializes empty instance. */
	CO2ComfortVentilation();

	/*! Destructor, writes out cached results from Therakles. */
	~CO2ComfortVentilation();

	/*! Initializes model */
	void init();

	/*! This function triggers a state-update of the embedded model whenever our cached input
		data differs from the input data in the model.
	*/
	void updateIfModified();

	/*! Called from fmi2DoStep(). */
	virtual void integrateTo(double tCommunicationIntervalEnd);

	// Functions for getting/setting the state

	/*! This function computes the size needed for full serizalization of
		the FMU and stores the size in m_fmuStateSize.
		\note The size includes the leading 8byte for the 64bit integer size
		of the memory array (for testing purposes).
	*/
	virtual void computeFMUStateSize();

	/*! Copies the internal state of the FMU to the memory array pointed to by FMUstate.
		Memory array always has size m_fmuStateSize.
	*/
	virtual void serializeFMUstate(void * FMUstate);

	/*! Copies the content of the memory array pointed to by FMUstate to the internal state of the FMU.
		Memory array always has size m_fmuStateSize.
	*/
	virtual bool deserializeFMUstate(void * FMUstate);

private:
	/*! Initialization function:*/
	/*!	Reads data from project file*/
	void read(const Path &fpath);

	/*! Calculation function:*/
	/*!	Digital/hysteresis controller for air change rate*/
	void calculateAirChangeRate(double &airChangeRate, double airTemperature, double ambientTemperature, double CO2Concentration);

	/*! static constants */

	/*! Ideal gas constant [J/molK]*/
	static const double RIdealGas;
	/*! Molar mass of CO2 [kg/mol]*/
	static const double molarMassCO2;
	/*! Reference pressure [Pa]*/
	static const double referencePressure;
	/*! Newton convergence constant*/
	static const double EPS;
	/*! MAximum Newton iterations*/
	static const unsigned int MAX_ITER;
	/*! Number of numerical integration intervals*/
	static const unsigned int NUM_INT_INTVALS;


	/*! Parameters*/

	/*! Number of zones*/
	unsigned int				m_nZones = 999;
	/*! Zone air volume [m3], sorted by zone ids*/
	std::map<int, double>		m_zoneVolumes;
	/*! Zone floor area [m2], sorted by zone ids*/
	std::map<int, double>		m_zoneFloorAreas;
	/*! Zone schedule name, sorted by zone ids*/
	std::map<int, std::string>	m_zoneScheduleNames;
	/*! Spline values sorted by schedule name*/
	std::map<std::string, LinearSpline>	m_CO2SourcePerZoneFloorAreasSplines;
	/*! Ambient CO2 concentration [mol/mol]*/
	double						m_ambientCO2Concentration = -999;
	/*! Start value for zone air CO2 concentration [mol/mol]*/
	double						m_startCO2Concentration = -999;
	/*! Maximum zone air CO2 concentration, before ventilation is activated [mol/mol]*/
	double						m_maximumCO2Concentration = -999;
	/*! Minimum zone air temperature, before heating ventilation is activated [K]*/
	double						m_minimumAirTemperature = -999;
	/*! Maximum zone air temperature, before cooling ventilation is activated [K]*/
	double						m_maximumAirTemperature = -999;
	/*! Start value for zone air temperature [K]*/
	double						m_startAirTemperature = -999;
	/*! Tolerance band for hystersis controller for CO2 concentration [mol/mol]
		(if deactivated, digital controller is chosen)*/
	double						m_CO2ToleranceBand = 0;
	/*! Tolerance band for hystersis controller for zone air temperature [K]
		(if deactivated, digital controller is chosen)*/
	double						m_temperatureToleranceBand = 0;
	/*! Minimum air change.*/
	double						m_minimumAirChangeRate = -999;
	/*! Maximum air change rate for control.*/
	double						m_maximumAirChangeRate = -999;

	/*! Time integration quantities*/

	/*! Cached current time point of the FMU, defines starting point for time integration in co-simulation mode. */
	double m_currentTimePoint;

	/*! Last time point*/
	double						m_lastTimePoint;

	/*! Solution quantities*/

	/*! Vector of CO2 masses.*/
	std::map<int, double>		m_zoneCO2Masses;
	/*! Vector of CO2 masses of the previos time point*/
	std::map<int, double>		m_zoneCO2MassesLastTimePoint;
	/*! Vector of calculated air change rates*/
	std::map<int, double>		m_zoneAirChangeRates;
}; // class CO2ComfortVentilation

#endif // CO2ComfortVentilationH

