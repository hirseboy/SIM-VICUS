#ifndef SVPropModeSelectionWidgetH
#define SVPropModeSelectionWidgetH

#include <QWidget>

namespace Ui {
	class SVPropModeSelectionWidget;
}

class ModificationInfo;


/*! This widget is shown on top of the property widget page when in property-edit mode. */
class SVPropModeSelectionWidget : public QWidget {
	Q_OBJECT

public:
	explicit SVPropModeSelectionWidget(QWidget *parent = nullptr);
	~SVPropModeSelectionWidget();

	/*! Called to update combo boxes. */
	void updateUI();

	/*! Based on selected properties, switch to one of the specific edit modes, when still
		in "default" mode.
	*/
	void selectionChanged();

signals:

	/*! Emitted when user has selected the site properties. */
	void sitePropertiesSelected();

	/*! Emitted when user activates building properties combo box or changes
		the selection in the combo box.
		\param propertyIndex Index of the selected property in the combo box
	*/
	void buildingPropertiesSelected(int propertyIndex);

	/*! Emitted when user activates network properties combo box or changes
		the selection in the combo box.
		\param networkIndex Index of the selected network in the combo box, may be -1 if empty.
		\param propertyIndex Index of the selected network property in the combo box, 0 = network, 1 = node, 2 = element.
	*/
	void networkPropertiesSelected(int networkIndex, int propertyIndex);

private slots:
	void on_pushButtonBuilding_toggled(bool checked);

	void on_pushButtonNetwork_toggled(bool checked);

	void on_pushButtonSite_toggled(bool checked);

	void on_comboBoxNetwork_currentIndexChanged(int index);

	void on_comboBoxNetworkProperties_currentIndexChanged(int index);

	void on_comboBoxBuildingProperties_currentIndexChanged(int index);

private:
	/*! This is called whenever the user has made changes to any of the components in
		this widget and updates the enabled/disabled states of all controls.
	*/
	void updateProperties();

	Ui::SVPropModeSelectionWidget *m_ui;
};


#endif // SVPropModeSelectionWidgetH
