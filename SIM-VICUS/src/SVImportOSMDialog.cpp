#include "SVImportOSMDialog.h"
#include "ui_SVImportOSMDialog.h"
#include "SVSettings.h"

#include <QtNetwork>
#include <QFileDialog>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QVariantMap>
#include <QApplication>
#include <QPushButton>

SVImportOSMDialog::SVImportOSMDialog(QWidget *parent)
	: QDialog(parent)
	, m_ui(new Ui::SVImportOSMDialog)
{
	m_ui->setupUi(this);

	connect(m_ui->buttonBox, &QDialogButtonBox::accepted, this, &SVImportOSMDialog::on_buttonbox_accepted);
	connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &SVImportOSMDialog::on_buttonbox_cancel);
	m_importButton = m_ui->buttonBox->button(QDialogButtonBox::Ok);
	m_importButton->setText("Import");
	initialise();
	m_progressNotifyer = new OSMImportProgressNotifyer();
	m_progressNotifyer->m_prgBar = m_ui->progressBar;
	m_timer = new QTimer(this);
	m_ui->widgetBrowseFilename->setup(m_downloadFilePath, true, true, tr("OSM files (*.osm *.vicosm);;All files (*.*)"), true);
	connect(m_ui->widgetBrowseFilename, &QtExt::BrowseFilenameWidget::editingFinished, this, &SVImportOSMDialog::on_widgetBrowseFilename_changed);
}

SVImportOSMDialog::~SVImportOSMDialog()
{
	delete m_ui;
}

bool SVImportOSMDialog::import()
{
	initialise();
	return (bool)exec();
}

void SVImportOSMDialog::on_buttonbox_accepted() {
	if (m_selectFromMap) {
		m_fname = QString("/home/sandisk/SHK/downloaded_map.osm");
	} else {
		m_fname = m_ui->widgetBrowseFilename->filename();
	}
}

void SVImportOSMDialog::on_buttonbox_cancel()
{
	if (m_process && m_process->state() != QProcess::NotRunning) {
		m_process->kill();
	}
	QFile file(m_downloadFilePath);
	if (file.exists()) {
		file.remove();
	}
}

void SVImportOSMDialog::downloadOsmFile(double minlon, double minlat, double maxlon, double maxlat)
{
	// Construct the URL for the OSM file
	QString url = QString("https://overpass-api.de/api/map?bbox=%1,%2,%3,%4")
					  .arg(minlon)
					  .arg(minlat)
					  .arg(maxlon)
					  .arg(maxlat);


	if (m_process != nullptr)
		delete m_process;

	QFile file(m_downloadFilePath);
	if (file.exists())
		file.remove();

	m_process = new QProcess();
	m_process->setWorkingDirectory("/home/sandisk/SHK");
	QString binary = "/usr/bin/curl";
	QStringList arguments;
	arguments << "-o" << m_downloadFilePath << url;

	m_process->start(binary, arguments);

	QString fileSizeInMb = QString("Download in Progress: %1 MB").arg(0/* 1 MB */ );
	m_ui->plainTextEditLog->appendPlainText(fileSizeInMb);
	connect(m_timer, &QTimer::timeout, this, &SVImportOSMDialog::updateDownloadProgress);
	m_timer->start(100);
}

void SVImportOSMDialog::initialise()
{
	m_importButton->setEnabled(false);
	m_ui->widgetBrowseFilename->setFilename("");
	m_selectFromMap = false;
	m_ui->lineEditBoundingBox->clear();
	m_ui->checkBoxToggle3D->setEnabled(true);
	on_radioButtonImportFile_toggled(true);
	m_ui->radioButtonImportFile->setChecked(true);
	on_radioButtonDownloadOSM_toggled(false);
	m_ui->radioButtonImportFile->setEnabled(true);
	m_ui->radioButtonDownloadOSM->setEnabled(true);
	m_ui->plainTextEditLog->clear();
	m_ui->progressBar->reset();
	m_ui->progressBar->setEnabled(false);
	m_ui->lineEditBoundingBox->clear();
}

void SVImportOSMDialog::createQml()
{
	if (m_engine == nullptr) {
		m_engine = new QQmlEngine(this);
		if (m_component != nullptr)
			delete m_component;
		m_component = new QQmlComponent(m_engine, QUrl("qrc:/qml/SVImportOSMMap.qml"));
	}

	m_activeObject = m_component->create();

	if (m_activeObject) {
		// Connect the QML signals to the C++ slots
		connect(m_activeObject, SIGNAL(okClicked()), this, SLOT(on_qmlOK_clicked()));
		connect(m_activeObject, SIGNAL(cancelClicked()), this, SLOT(on_qmlCancel_clicked()));
	} else {
		qDebug() << "Failed to create the QML component instance.";
		foreach (const QQmlError &error, m_component->errors()) {
			qDebug() << error.toString();
		}
	}
}

void SVImportOSMDialog::updateDownloadProgress()
{
	QFileInfo fileInfo(m_downloadFilePath);
	qint64 currentSize = fileInfo.size();

	QString logText = m_ui->plainTextEditLog->toPlainText();
	QString searchString = "Download in Progress:";
	int lastIndex = logText.lastIndexOf(searchString);

	if (lastIndex != -1) {
		logText = logText.left(lastIndex) + QString("Download in Progress: %1 MB").arg(currentSize / 1048576.f/* 1 MB */ );
		m_ui->plainTextEditLog->setPlainText(logText);
	}

	//QString fileSizeInMb = QString("Download in Progress: %1 MB").arg(currentSize / 1048576.f/* 1 MB */ );
	//m_ui->plainTextEditLog->appendPlainText(fileSizeInMb);

	if (m_process->state() == QProcess::NotRunning) {
		m_timer->stop();
		m_progressNotifyer->disableAnimation();
		readAndConstructOSM();
	}
}

bool SVImportOSMDialog::readAndConstructOSM()
{
	m_ui->widgetLocal->setEnabled(false);
	m_ui->widgetDownload->setEnabled(false);
	m_ui->radioButtonDownloadOSM->setEnabled(false);
	m_ui->radioButtonImportFile->setEnabled(false);
	m_ui->checkBoxToggle3D->setEnabled(false);
	m_progressNotifyer->enablePrgBar(true);
	m_drawingOSM = VICUS::DrawingOSM();
	m_drawingOSM.m_enable3DBuildings = m_ui->checkBoxToggle3D->isChecked();

	if (m_fname.contains(".osm")) {
		m_ui->plainTextEditLog->appendPlainText("Attempt to read .osm");
		if (!m_drawingOSM.import(m_fname, m_progressNotifyer)) {
			m_ui->widgetLocal->setEnabled(m_ui->radioButtonImportFile->isChecked());
			m_ui->widgetDownload->setEnabled(m_ui->radioButtonDownloadOSM->isChecked());
			m_ui->radioButtonDownloadOSM->setEnabled(true);
			m_ui->radioButtonImportFile->setEnabled(true);
			m_ui->checkBoxToggle3D->setEnabled(true);
			m_progressNotifyer->enablePrgBar(false);
			m_ui->plainTextEditLog->appendPlainText("Reading OSM Failed");
		}
		m_ui->plainTextEditLog->appendPlainText("Reading OSM Successful");
	} else if (m_fname.contains("vicosm")) {
		TiXmlDocument document(m_fname.toStdString());
		if (!document.LoadFile()) {
			qDebug() << "Failed to load file: " << m_fname;
			qDebug() << "Error: " << document.ErrorDesc();
			return false;
		}

		TiXmlElement* root = document.RootElement();
		if (!root) {
			qDebug() << "Failed to get root element";
			return false;
		}
		// Print the root element's name
		qDebug() << "Root element: " << root->Value();
		const TiXmlElement * c = document.FirstChildElement();
		const TiXmlElement * d = c->FirstChildElement();
		m_drawingOSM.readXML(d->FirstChildElement());
	} else {
		return false;
	}

	m_ui->plainTextEditLog->appendPlainText("Constructing Nodes, Ways, Relations");
	m_drawingOSM.constructObjects(m_progressNotifyer);
	m_importButton->setEnabled(true);
	m_progressNotifyer->notify(100, "Successful");
	m_ui->plainTextEditLog->appendPlainText("Construction finished");
	return true;
}

void SVImportOSMDialog::on_toolButtonMap_clicked()
{
	createQml();
}

void SVImportOSMDialog::on_qmlOK_clicked()
{
	if (m_activeObject) {
		QQuickItem *mapObject = m_activeObject->findChild<QQuickItem*>("mapObject");
		if (mapObject) {
			// Call the getBoundingBox method from QML
			QVariant returnedValue;
			QMetaObject::invokeMethod(mapObject, "getBoundingBox",
									  Q_RETURN_ARG(QVariant, returnedValue));
			QVariantMap boundingBox = returnedValue.toMap();
			qDebug() << "Bounding Box:" << boundingBox;

			// call downloadOsmFile: Bounding Box: QMap(("northWest", QVariant(QVariantMap, QMap(("latitude", QVariant(double, 52.5358))("longitude", QVariant(double, 13.3253)))))("southEast", QVariant(QVariantMap, QMap(("latitude", QVariant(double, 52.4968))("longitude", QVariant(double, 13.4302))))))
			QVariantMap northWest = boundingBox["northWest"].toMap();
			QVariantMap southEast = boundingBox["southEast"].toMap();

			double minlon = northWest["longitude"].toDouble();
			double minlat = southEast["latitude"].toDouble();
			double maxlon = southEast["longitude"].toDouble();
			double maxlat = northWest["latitude"].toDouble();
			QMetaObject::invokeMethod(m_activeObject, "close");
			m_activeObject = nullptr;

			QString boundingBoxString = QString("%1, %2, %3, %4").arg(minlon)
									   .arg(minlat)
									   .arg(maxlon)
									   .arg(maxlat);
			m_ui->lineEditBoundingBox->setText(boundingBoxString);
			m_ui->plainTextEditLog->appendPlainText("Bounding Box selected: " + boundingBoxString);
			double minx, miny, maxx, maxy;
			int utmZone = 0;
			IBKMK::LatLonToUTMXY(minlat, minlon, utmZone, minx, miny);
			IBKMK::LatLonToUTMXY(maxlat, maxlon, utmZone, maxx, maxy);
			double euclideanDistance = sqrt(pow(maxx - minx, 2) + pow(maxy - miny, 2));
			if (euclideanDistance > 100000) {
				m_ui->plainTextEditLog->appendPlainText("Warning: The selected area is very large. Download can be stopped by closing this Dialog.");
			}
			m_importButton->setEnabled(false);


			m_progressNotifyer->enableAnimation();
			downloadOsmFile(minlon, minlat, maxlon, maxlat);
			m_selectFromMap = true;
			m_fname = m_downloadFilePath;
			return;
		}
	} else {

	}
	QMetaObject::invokeMethod(m_activeObject, "close");
	m_activeObject = nullptr;
}

void SVImportOSMDialog::on_qmlCancel_clicked()
{
	if (m_activeObject) {
		QMetaObject::invokeMethod(m_activeObject, "close");
		m_activeObject = nullptr;
	}
}

void SVImportOSMDialog::on_radioButtonImportFile_toggled(bool checked)
{
	m_ui->widgetLocal->setEnabled(checked);
	if (checked)
		on_widgetBrowseFilename_changed();
}

void SVImportOSMDialog::on_radioButtonDownloadOSM_toggled(bool checked)
{
	m_ui->widgetDownload->setEnabled(checked);
}

void SVImportOSMDialog::on_widgetBrowseFilename_changed()
{
	const QString& filename = m_ui->widgetBrowseFilename->filename();
	if (filename.endsWith(".osm")) {
		m_ui->plainTextEditLog->appendPlainText(QString("New local filename set: %1").arg(filename));
		m_importButton->setEnabled(false);
		m_fname = m_ui->widgetBrowseFilename->filename();
		readAndConstructOSM();
	}
}

void OSMImportProgressNotifyer::notify(double value, const char * text) {
	m_prgBar->setFormat(QString::fromStdString(std::string(text)));
	m_prgBar->setValue(value);
	qApp->processEvents();
}

void OSMImportProgressNotifyer::notifyDownloadProgress(QString text) {
	m_prgBar->setFormat(text);
	qApp->processEvents();
}

void OSMImportProgressNotifyer::enableAnimation() {
	m_prgBar->setEnabled(true);
	m_prgBar->setRange(0,0);
	m_prgBar->setAlignment(Qt::AlignCenter);
	m_prgBar->setTextVisible(true);
}

void OSMImportProgressNotifyer::disableAnimation()
{
	m_prgBar->setRange(0, 100);
	m_prgBar->setValue(30);
}

void OSMImportProgressNotifyer::enablePrgBar(bool enable)
{
	m_prgBar->setEnabled(enable);
}