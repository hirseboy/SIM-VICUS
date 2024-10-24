#ifndef SVPropBuildingZoneTemplatesWidgetH
#define SVPropBuildingZoneTemplatesWidgetH

#include <QWidget>

#include <VICUS_Constants.h>


namespace Ui {
	class SVPropBuildingZoneTemplatesWidget;
}

namespace VICUS {
	class ZoneTemplate;
	class Room;
}

class QTableWidgetItem;

/*! Widget to manage assigned zone templates. */
class SVPropBuildingZoneTemplatesWidget : public QWidget {
	Q_OBJECT

public:
	explicit SVPropBuildingZoneTemplatesWidget(QWidget *parent = nullptr);
	~SVPropBuildingZoneTemplatesWidget();

	/*! Updates user interface. */
	void updateUi();

private slots:
	void on_pushButtonAssignZoneTemplate_clicked();
	void on_tableWidgetZoneTemplates_itemSelectionChanged();
	/* Triggers the openEditZoneTemplatesDialog*/
	void on_pushButtonEditZoneTemplates_clicked();
	void on_pushButtonExchangeZoneTemplates_clicked();
	void on_checkBoxZoneTemplateColorOnlyActive_toggled(bool checked);
	void on_checkBoxZoneTemplateShowOnlyActive_toggled(bool checked);
	void on_tableWidgetZoneTemplates_itemClicked(QTableWidgetItem *item);

	/*! All rooms (including surfaces) that reference the currently selected zone template (in the table) will be selected. */
	void on_pushButtonSelectObjectsWithZoneTemplate_clicked();


	void on_pushButtonAssignSelectedZoneTemplate_clicked();

	/* Triggers the openEditZoneTemplatesDialog*/
	void on_tableWidgetZoneTemplates_cellDoubleClicked(int, int);

private:
	/*! Returns a pointer to the currently selected zone template in the zone template table. */
	const VICUS::ZoneTemplate * currentlySelectedZoneTemplate() const;

	/*! Triggered when show-only-active check box was checked and table selection has changed,
		sends a recolor signal.
	*/
	void zoneTemplateColoredZoneSelectionChanged();

	/*! Triggered when select-active check box was checked and table selection has changed,
		modifies visibility state of respective room nodes (and their surfaces).
	*/
	void zoneTemplateVisibleZonesSelectionChanged();

	/*! Launches ZoneTemplates db edit dialog. */
	void openEditZoneTemplatesDialog();

	Ui::SVPropBuildingZoneTemplatesWidget *m_ui;

	/*! Maps stores pointers to room objects grouped for assigned zone templates.
		Note: rooms without zone template ID are ignored.
	*/
	std::map<const VICUS::ZoneTemplate*, std::vector<const VICUS::Room *> >			m_zoneTemplateAssignments;

	/*! Zone template id currently selected in the scene. */
	unsigned int																	m_selectedZoneTemplateId = VICUS::INVALID_ID;
};

#endif // SVPropBuildingZoneTemplatesWidgetH
