#ifndef SVDBScheduleEditWidget_H
#define SVDBScheduleEditWidget_H

#include <QWidget>


namespace VICUS {
	class Schedule;
	class ScheduleInterval;
}

class SVDBScheduleTableModel;
class SVDatabase;


namespace Ui {
class SVDBScheduleEditWidget;
}
/*! Edit widget for schedules.

	A call to updateInput() initializes the widget and fill the GUI controls with data.
	As long as the widget is visible the pointer to the data must be valid. Keep this
	in mind if you change the container that the data object belongs to! If the pointer
	is no longer valid or you want to resize the container (through adding new items)
	call updateInput() with an invalid index and/or nullptr pointer to the model.
*/
class SVDBScheduleEditWidget : public QWidget{
	Q_OBJECT

public:
	explicit SVDBScheduleEditWidget(QWidget *parent = nullptr);
	~SVDBScheduleEditWidget();

	/*! Needs to be called once, before the widget is being used. */
	void setup(SVDatabase * db, SVDBScheduleTableModel * dbModel);

	/*! Sets up the widget for a Schedule with a given ID. */
	void updateInput(int id);

private slots:

	void on_toolButtonAddPeriod_clicked();

	void on_toolButtonRemovePeriode_clicked();

	void on_tableWidgetPeriods_currentCellChanged(int currentRow, int, int, int);

private:
	/*! Update the period table. */
	void updatePeriodTable();
	/*! Called when a new daily cycle has been selected,i.e. m_currentDailyCycleIndex has changed. */
	void selectDailyCycle();



	Ui::SVDBScheduleEditWidget			*m_ui;

	/*! Cached pointer to database object. */
	SVDatabase							*m_db;

	/*! Pointer to the database model, to modify items when data has changed in the widget. */
	SVDBScheduleTableModel				*m_dbModel;

	/*! Pointer to currently edited schedule.
		The pointer is updated whenever updateInput() is called.
		A nullptr pointer means that there is no schedule to edit.
	*/
	VICUS::Schedule						*m_current;

	/*! Pointer to currently selected interval, updated in on_tableWidgetPeriods_currentCellChanged(). */
	VICUS::ScheduleInterval				*m_currentInterval;
	/*! Index of currently selected daily cycle, initialized with 0 in on_tableWidgetPeriods_currentCellChanged(), modified with arrow buttons. */
	unsigned int						m_currentDailyCycleIndex;

	/*! Actual row index of period table. (-1 no row selected)*/
	int									m_rowIdx=-1;



};

//class SVDBScheduleDailyCycleEditWidget : public QWidget{
//	Q_OBJECT

//public:
//	explicit SVDBScheduleDailyCycleEditWidget(QWidget *parent = nullptr);
//	~SVDBScheduleDailyCycleEditWidget();

//};

#endif // SVDBScheduleEditWidget_H
