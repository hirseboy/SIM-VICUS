#ifndef SVPROPRESULTSVIEWWIDGETH
#define SVPROPRESULTSVIEWWIDGETH

#include <QWidget>
#include <QDir>
#include <QDateTime>

#include "NANDRAD_LinearSplineParameter.h"

namespace Ui {
class SVPropResultsWidget;
}

class ModificationInfo;

/*! Widget showing options to highlight results in false color mode in UI. */
class SVPropResultsWidget : public QWidget {
	Q_OBJECT

public:
	explicit SVPropResultsWidget(QWidget *parent = nullptr);
	~SVPropResultsWidget() override;

	/*! This function is called whenever the property widget is shown.
		It initializes the default results directory on first call,
		and afterwards re-reads the output directory as if user would
		have pressed "Refresh" button manually.
	*/
	void refreshDirectory();

private slots:

	void onModified(int modificationType, ModificationInfo *data);

	void onTimeSliderCutValueChanged(double currentTime);

	void on_pushButtonMaxColor_clicked();

	void on_pushButtonMinColor_clicked();

	void on_pushButtonSetGlobalMinMax_clicked();

	void on_pushButtonSetLocalMinMax_clicked();

	void on_tableWidgetAvailableResults_cellDoubleClicked(int row, int column);

	void on_toolButtonSetDefaultDirectory_clicked();

	void on_pushButtonRefreshDirectory_clicked();

	void on_lineEditMaxValue_editingFinishedSuccessfully();

	void on_lineEditMinValue_editingFinishedSuccessfully();

	void on_comboBoxPipeType_activated(int index);

private:

	/*! Caches the content of a single output file. */
	struct ResultDataSet {
		enum FileStatus {
			/*! File hasn't been read, yet. */
			FS_Unread,
			/*! File on disk is older/same age as m_timeStampLastUpdated */
			FS_Current,
			/*! File on disk is newer than read data set, update needed. */
			FS_Outdated,
			/*! When file cannot be read (again) or file is missing. */
			FS_Missing
		};

		/*! File name, relative to 'results/' directory. */
		QString		m_filename;
		/*! Stores date/time when data from this file was read last.
			Time stamp is invalid, if data hasn't been read, yet.
		*/
		QDateTime	m_timeStampLastUpdated;

		/*! The file status. */
		FileStatus	m_status = FS_Unread;
	};

	/*! Parses the substitution file ('objectref_substitutions.txt') and the header of all tsv files in the currently selected results directory.
		Updates the table widget if results have been found */
	void readResultsDir();

	/*! Parses the entire tsv file of the currently selected output property. Stores all outputs that have been found in this tsv file
	 *  (not only the currently selected one) in the m_allResults map. Also indicates those outputs through green flag in the table widget. */
	void readCurrentResult(bool forceToRead);

	/*! Determine min/max values of current output. If localMinMax==true, the min/max of current time point are determined, otherwise the min/max of entire spline are determined */
	void setCurrentMinMaxValues(bool localMinMax=false);

	/*! Interpolates given color for given value using the current min/max values and colors. Uses linear HSV interpolation */
	void interpolateColor(const double &val, QColor &col);

	/*! Sets colors to all VICUS objects, based on previously found ids */
	void updateColors(const double & currentTime);

	Ui::SVPropResultsWidget							*m_ui;

	/*! Currently selected results directory, updated in readResultsDir(), does not contain trailing 'results/' subdirectory. */
	QDir											m_resultsDir;

	/*! List of all output files that are in resultsDir, updated in readResultsDir(). */
	QList<ResultDataSet>							m_outputFiles;

	/*! Maps an output variable (caption from tsv-file) to the file index in vector m_outputFiles. */
	std::map<QString, unsigned int>					m_outputVariables;

	/*! Stores VICUS Object Ids for each output property (effectively the content of the objectref_substitutions.txt file). */
	std::map<QString, unsigned int>					m_objectName2Id;

	/*! Holds map for each output property with key being VICUS Object Id and value being the according results values as linear spline. */
	std::map<QString, std::map<unsigned int, NANDRAD::LinearSplineParameter> >	m_allResults;

	/*! The currently selected output property. */
	QString											m_currentOutput;

	/*! The currently selected filter. */
	QString											m_currentFilter;

	/*! Min/Max values and colors. */
	double											m_currentMin = 0;
	double											m_currentMax = 1;
	QColor											m_minColor;
	QColor											m_maxColor;
};

#endif // SVRESULTSVIEWWIDGETH
