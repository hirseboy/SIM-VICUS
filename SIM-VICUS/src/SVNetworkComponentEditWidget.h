/*	SIM-VICUS - Building and District Energy Simulation Tool.

	Copyright (c) 2020-today, Institut für Bauklimatik, TU Dresden, Germany

	Primary authors:
	  Andreas Nicolai  <andreas.nicolai -[at]- tu-dresden.de>
	  Dirk Weiss  <dirk.weiss -[at]- tu-dresden.de>
	  Stephan Hirth  <stephan.hirth -[at]- tu-dresden.de>
	  Hauke Hirsch  <hauke.hirsch -[at]- tu-dresden.de>

	  ... all the others from the SIM-VICUS team ... :-)

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

#ifndef SVDBNetworkComponentEditWidgetH
#define SVDBNetworkComponentEditWidgetH

#include "SVAbstractDatabaseEditWidget.h"

namespace Ui {
	class SVNetworkComponentEditWidget;
}

namespace VICUS {
	class NetworkComponent;
}

class SVDBNetworkComponentTableModel;
class SVNetworkControllerEditDialog;
class SVDatabase;
class QwtPlot;
class QwtPlotCurve;
class QwtPlotZoomer;

/*! Edit widget for network components.

	A call to updateInput() initializes the widget and fill the GUI controls with data.
	As long as the widget is visible the pointer to the data must be valid. Keep this
	in mind if you change the container that the data object belongs to! If the pointer
	is no longer valid or you want to resize the container (through adding new items)
	call updateInput() with an invalid index and/or nullptr pointer to the model.
*/
class SVNetworkComponentEditWidget: public QWidget {
	Q_OBJECT

public:

	enum DataType {
		DT_Integer,
		DT_DoubleStd,
		DT_DoubleAdditional,
		DT_DoubleOptional,
		NUM_DT
	};

	explicit SVNetworkComponentEditWidget(QWidget *parent = nullptr);
	~SVNetworkComponentEditWidget() ;

	/*! set current Component with this. */
	void updateInput(VICUS::NetworkComponent *component);

	/*! updates the Widget */
	void update();

	/*! updates pages */
	void updatePageHeatLossConstant();
	void updatePageHeatLossSpline();
	void updatePageTemperatureConstant(){}
	void updatePageTemperatureSpline(){}
	void updatePageTemperatureSplineEvaporator(){}
	void updatePageHeatingDemandSpaceHeating(){}

signals:
	void controllerChanged(QString controllerName);

private slots:
	void on_tableWidgetParameters_cellChanged(int row, int column);

	void on_toolButtonSchedule1_clicked();

	void on_toolButtonSchedule2_clicked();

	void on_toolButtonPipeProperties_clicked();

	void on_tableWidgetPolynomCoefficients_cellChanged(int row, int);

	void on_toolButtonControllerSet_clicked();

	void on_toolButtonControllerRemove_clicked();

	void on_comboBoxHeatExchange_activated(int index);

	void on_radioButtonHeatLossConstantPredef_clicked();

	void on_radioButtonHeatLossConstantUser_clicked();

	void on_lineEditHeatLossConstantUser_editingFinishedSuccessfully();

	void on_lineEditTemperatureConstantHeatTransferCoefficient_editingFinishedSuccessfully();

	void on_lineEditTemperatureConstantTemperature_editingFinishedSuccessfully();

	void on_checkBoxHeatLossSplineIndividual_clicked(bool checked);

	void on_comboBoxHeatLossSplineUserBuildingType_activated(int index);

	void on_checkBoxHeatLossSplineDemandCurveUserAreaRelatedValues_stateChanged(int arg1);

	void on_checkBoxHeatLossSplineDemandCurveUserWithCoolingDemand_stateChanged(int arg1);

	void on_lineEditHeatLossSplineFloorArea_editingFinishedSuccessfully();

	void on_horizontalSliderHeatLossSplinePlot_sliderMoved(int position);

	void on_lineEditHeatLossSplineMaximumHeatingLoad_editingFinishedSuccessfully();

	double maxYValueForMap();

	void on_lineEditHeatLossSplineHeatingEnergyDemand_editingFinishedSuccessfully();

	void on_checkBoxHeatLossSplineAreaRelatedValues_stateChanged(int arg1);

private:
	void updateParameterTableWidget() const;

	void updatePolynomCoeffTableWidget() const;

	void updatePolynomPlot();

	void handleTsv();

	double calculateHeatingEnergyDemand();

	void calculateNewHeatLossSplineYData(double k, std::vector<double>& vectorToSaveNewValues);

	Ui::SVNetworkComponentEditWidget		*m_ui;

	/*! Cached pointer to database object. */
	SVDatabase								m_db;

	/*! Dialog to create and edit Controller */
	SVNetworkControllerEditDialog			*m_controllerEditDialog = nullptr;

	/*! Flag to ensure only one dialog is spawned to request how to enforce HeatingEnergyDemand set by user */
	bool									m_isHeatingEnergyDemandDialogAlreadyOpen = false;

	/*! Pointer to currently edited component.
		The pointer is updated whenever updateInput() is called.
		A nullptr pointer means that there is no component to edit.
	*/
	VICUS::NetworkComponent					*m_current = nullptr;

	/*! The curve used to plot the polynoms. */
	std::vector<QwtPlotCurve*>				m_curves1;
	std::vector<QwtPlotCurve*>				m_curves2;

	/*! The data vectors needed for plotting. */
	std::vector<double>						m_xData;
	std::vector<std::vector<double>>		m_yData1;
	std::vector<std::vector<double>>		m_yData2;

	/*! Vector to temporarily hold all NetworkHeatExchange that were modified after a Block was selected in the scene */
	std::vector<VICUS::NetworkHeatExchange>	m_vectorTempHeatExchange;

	/*! Curve visible in HeatLossSpline plot */
	QwtPlotCurve							*m_heatLossSplineCurve = nullptr;
	QwtPlotZoomer							*m_heatLossSplineZoomer = nullptr;

	/*! Data vectors to store the original values of the tsv file */
	std::vector<double>						m_heatLossSplineXData;
	std::map<double, std::vector<double>>	m_mapHeatLossSplineYData;
	std::vector<double>						m_heatLossSplineMaxYValues;

	/*! Parameter to adjust curve to reach desired heatingEnergydemand */
	double									m_k = 1;

	/*! Data vectors to store values to be displayed in HeatLossSpline Plot */
	std::vector<double>						m_heatLossSplineXPlotData;
	std::vector<double>						m_heatLossSplineYPlotData;

};

#endif // SVDBNetworkComponentEditWidgetH
