/*	The NANDRAD data model library.

	Copyright (c) 2012-today, Institut für Bauklimatik, TU Dresden, Germany

	Primary authors:
	  Andreas Nicolai  <andreas.nicolai -[at]- tu-dresden.de>
	  Anne Paepcke     <anne.paepcke -[at]- tu-dresden.de>

	This library is part of SIM-VICUS (https://github.com/ghorwin/SIM-VICUS)

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.
*/

#ifndef NANDRAD_DailyCycleH
#define NANDRAD_DailyCycleH

#include <string>
#include <vector>

#include <IBK_Parameter.h>
#include "NANDRAD_LinearSplineParameter.h"
#include "NANDRAD_DataTable.h"
#include "NANDRAD_CodeGenMacros.h"

namespace NANDRAD {

/*!	Defines the daily course of one or more physical quantities/setpoints.

	The m_interpolation parameter defines how the tabulated data is interpreted.

	If m_interpolation == IT_CONSTANT, then the following rules apply:
	- the time points in m_timePoints are interpreted as *start* of the next interval
	- the first time point must be 0, all time points must be < 24 h
	- the corresponding value is taken as constant during this interval

	For example, a time point vector "0 6 18" defines three intervals: 0-6, 6-18, 18-24 and
	the data table must contain exactly 3 values.

	If m_interpolation == IT_LINEAR, then the following rules apply:
	- the time points in m_timePoints are points in time where associated values given
	- the first time point must be always 0, the last one must be < 24 h (< 1440 min, etc.),
	  because in cyclic usage, the time point at 24 h will be the same as for 0 h (and likewise
	  the scheduled values)
	- between time points the values are linearly interpolated
*/
class DailyCycle {
public:

	/*! Interpolation method for daily cycle data.
		Note that for constant values some ramping may be used to smoothen out the steps.
		If not set, IT_LINEAR is used.
	*/
	enum interpolation_t {
		IT_CONSTANT,	// Keyword: Constant	'Constant values in defined intervals.'
		IT_LINEAR,		// Keyword: Linear		'Linear interpolation between values.'
		NUM_IT
	};

	// *** PUBLIC MEMBER FUNCTIONS ***

	NANDRAD_READWRITE
	NANDRAD_COMP(DailyCycle)

	/*! Checks for valid parametrization and generates m_valueNames, m_valueUnits and m_valueData. */
	void prepareCalculation();

	// *** PUBLIC MEMBER VARIABLES ***

	interpolation_t						m_interpolation = NUM_IT;			// XML:A

	/*! Time points, first must be 0, last must be end of day when IT_LINEAR is used. */
	std::vector<double>					m_timePoints;						// XML:E
	IBK::Unit							m_timeUnit;							// XML:E

	/*! Actual values, key of m_values.m_values is physical quantity/setpoint/... scheduled quantity, value is vector with values, same
		number of values as in m_timePoints.
	*/
	DataTable							m_values;							// XML:E

	// *** solver runtime variables only (not written to file) ***

	std::vector<std::string>			m_valueNames;
	std::vector<IBK::Unit>				m_valueUnits;
};

} // namespace NANDRAD

#endif // NANDRAD_DailyCycleH
