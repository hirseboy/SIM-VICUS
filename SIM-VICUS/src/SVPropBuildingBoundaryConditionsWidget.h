#ifndef SVPropBuildingBoundaryConditionsWidgetH
#define SVPropBuildingBoundaryConditionsWidgetH

#include <QWidget>

#include <map>
#include <set>

namespace Ui {
	class SVPropBuildingBoundaryConditionsWidget;
}

namespace VICUS {
	class BoundaryCondition;
	class Surface;
}

/*! A table showing the used boundary condition models.

	All visible surfaces are inspected and grouped according to their usage in ComponentInstances.
	A map is created of Components and surfaces, that use these components.
	Then, the boundary conditions in these components are inspected and a map is created
	for BoundaryConditions vs. surfaces. This is then shown in the table.
*/
class SVPropBuildingBoundaryConditionsWidget : public QWidget {
	Q_OBJECT

public:
	explicit SVPropBuildingBoundaryConditionsWidget(QWidget *parent = nullptr);
	~SVPropBuildingBoundaryConditionsWidget();

	/*! Updates user interface. */
	void updateUi();

private slots:
	void on_pushButtonEditBoundaryConditions_clicked();
	void on_tableWidgetBoundaryConditions_itemSelectionChanged();

	void on_pushButtonSelectBoundaryConditions_clicked();

private:
	Ui::SVPropBuildingBoundaryConditionsWidget *m_ui;

	/*! Map that holds the list of used boundary conditions vs. visible surfaces. */
	std::map<const VICUS::BoundaryCondition *, std::set<const VICUS::Surface *> >	m_bcSurfacesMap;
};

#endif // SVPropBuildingBoundaryConditionsWidgetH