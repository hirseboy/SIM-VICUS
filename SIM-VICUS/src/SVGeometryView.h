#ifndef SVGeometryViewH
#define SVGeometryViewH

#include <QWidget>


namespace Vic3D {
	class SceneView;
}

class SVPropertyWidget;
class QSplitter;
class QToolBar;
class QAction;

/*! The main geometry view.
	The 3D scene view is embedded into this widget.
*/
class SVGeometryView : public QWidget {
	Q_OBJECT
public:

	explicit SVGeometryView(QWidget *parent = nullptr);

	void saveScreenShot(const QString & imgFilePath);

	/*! Provides read-only access to sceneView() so that signals can be connected. */
	const Vic3D::SceneView * sceneView() const { return m_sceneView; }

public slots:
	/*! Connected to view-state handler. */
	void onViewStateChanged();

private:
	void setupActions();

	/*! The scene view, that shows our world and allows navigation */
	Vic3D::SceneView			*m_sceneView								= nullptr;
	/*! The property widget is located to the right of the view and is layouted in a splitter. */
	SVPropertyWidget			*m_propertyWidget							= nullptr;
	/*! Splitter that contains the scene view and the property widget. */
	QSplitter					*m_splitter									= nullptr;


	QToolBar					*m_toolBar									= nullptr;

	QAction						*m_snapAction								= nullptr;
	QAction						*m_xLockAction								= nullptr;
	QAction						*m_yLockAction								= nullptr;
	QAction						*m_zLockAction								= nullptr;
};

#endif // SVGeometryViewH
