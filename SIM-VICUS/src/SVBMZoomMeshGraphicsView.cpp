/*	BSD 3-Clause License

	This file is part of the BlockMod Library.

	Copyright (c) 2019, Andreas Nicolai
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	   list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its
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

#include "SVBMZoomMeshGraphicsView.h"

#include <QWheelEvent>
#include <QList>
#include <QGraphicsItem>
#include <QDebug>
#include <QApplication>
#include <QMimeData>

#include <cmath>

#include "SVBMSceneManager.h"
#include "VICUS_BMNetwork.h"
#include "VICUS_BMGlobals.h"


SVBMZoomMeshGraphicsView::SVBMZoomMeshGraphicsView(QWidget *parent) :
	QGraphicsView(parent),
	m_resolution(10000), // 1000 px/m
	m_gridStep(100),
	m_gridEnabled( true ),
	m_zoomLevel(0),
	m_gridColor( 175, 175, 255 ),
	m_gridSpacingPixLast(0),
	m_sceneManager(new SVBMSceneManager(this))
{
	setTransformationAnchor(AnchorUnderMouse);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	setViewportUpdateMode( QGraphicsView::FullViewportUpdate );
	setResolution(1);
	setAcceptDrops(true);
	setScene(m_sceneManager);
}



void SVBMZoomMeshGraphicsView::wheelEvent(QWheelEvent *i_event){

	if (i_event->angleDelta().y() < 0) {
		zoomOut();
	}
	else {
		zoomIn();
	}
	i_event->accept();
}



void SVBMZoomMeshGraphicsView::paintEvent(QPaintEvent *i_event){

	if (m_gridEnabled) {

		// get rect
		QPainter p(viewport());
		QPointF p1 = mapFromScene( QPointF(0,0) );

		// compute nominal pixels using resolution
		double gridSpacingPix = m_resolution*m_gridStep;

		// apply scaling
		double scaleFactor = transform().m11();
		gridSpacingPix *= scaleFactor;

		// get dimensions
		int h = height();
		int w = width();
		QSize windowSize(w,h);

		// if new reference point, grid scale factor and window size still match cached values, do
		// not recompute grid lines
		if (gridSpacingPix != m_gridSpacingPixLast ||
			p1 != m_pLast ||
			windowSize != m_windowSizeLast)
		{

			// cache values used for calculation
			m_pLast = p1;
			m_gridSpacingPixLast = gridSpacingPix;
			m_windowSizeLast = windowSize;

			// *** major grid ***

			// clear major grid lines
			m_majorGrid.clear();

			// only regenerate mesh and draw grid if spacing is big enough
			if (gridSpacingPix >= 5) {

				// calculate offset - we start at 0 pix in scaled scene coordinates
				int linesXTillView = std::floor(-p1.x()/gridSpacingPix + 1);
				double offsetX = linesXTillView*gridSpacingPix + p1.x();

				int linesYTillView = std::floor(-p1.y()/gridSpacingPix + 1);
				double offsetY = linesYTillView*gridSpacingPix + p1.y();

				for (double x = offsetX; x < w; x += gridSpacingPix) {
					m_majorGrid.append(QLineF(x, 0, x, h));
				}
				for (double y = offsetY; y < h; y += gridSpacingPix) {
					m_majorGrid.append(QLineF(0, y, w, y));
				}
			}


			// *** minor grid ***

			// compute nominal pixels using resolution, this time for minor grid
			gridSpacingPix = m_resolution*m_gridStep*0.1;

			// apply scaling
			scaleFactor = transform().m11();
			gridSpacingPix *= scaleFactor;

			// clear minor grid lines
			m_minorGrid.clear();

			// only draw grid if spacing is big enough
			if (gridSpacingPix >= 5) {

				// calculate offset - we start at 0 pix in scaled scene coordinates
				int linesXTillView = std::floor(-p1.x()/gridSpacingPix + 1);
				double offsetX = linesXTillView*gridSpacingPix + p1.x();

				int linesYTillView = std::floor(-p1.y()/gridSpacingPix + 1);
				double offsetY = linesYTillView*gridSpacingPix + p1.y();

				// create a vector of lines to draw
				m_minorGrid.clear();

				for (double x = offsetX; x < w; x += gridSpacingPix) {
					m_minorGrid.append(QLineF(x, 0, x, h));
				}
				for (double y = offsetY; y < h; y += gridSpacingPix) {
					m_minorGrid.append(QLineF(0, y, w, y));
				}

			}
		}
		// we paint in view coordinates
		p.setPen( QColor(220,220,255) );
		p.drawLines(m_minorGrid.data(), m_minorGrid.size());

		// we paint in view coordinates
		p.setPen( m_gridColor );
		p.drawLines(m_majorGrid.data(), m_majorGrid.size());

	}

	QGraphicsView::paintEvent(i_event);
}


void SVBMZoomMeshGraphicsView::enterEvent(QEvent *event) {
	Q_ASSERT(event->type() == QEvent::Enter);

	SVBMSceneManager * sceneManager = qobject_cast<SVBMSceneManager *>(scene());
	if (sceneManager) {
		// clear any override cursors that we had enabled when leaving the scene
		while (QApplication::overrideCursor() != nullptr)
			QApplication::restoreOverrideCursor();
	}
	QGraphicsView::enterEvent(event);
}


void SVBMZoomMeshGraphicsView::leaveEvent(QEvent *event) {
	SVBMSceneManager * sceneManager = qobject_cast<SVBMSceneManager *>(scene());
	if (sceneManager) {
		if (sceneManager->isCurrentlyConnecting())
			sceneManager->finishConnection();
	}
	QApplication::restoreOverrideCursor(); // needed if we drag a segment out of the view
	QGraphicsView::leaveEvent(event);
}


void SVBMZoomMeshGraphicsView::mouseMoveEvent(QMouseEvent *i_event) {
	QGraphicsView::mouseMoveEvent(i_event);
	m_pos = mapToScene(i_event->pos());
	viewport()->update();
}


void SVBMZoomMeshGraphicsView::setGridColor( QColor color ) {
	m_gridColor = color;
	viewport()->update();
}


void SVBMZoomMeshGraphicsView::setGridEnabled( bool enabled ) {
	m_gridEnabled = enabled;
	viewport()->update();
}


void SVBMZoomMeshGraphicsView::zoomIn() {

	if(m_zoomLevel  >= 10) return;
	m_zoomLevel = std::min(m_zoomLevel+1, 3000);
	double factor = std::pow(10,m_zoomLevel/20.0);
	QTransform m(factor, 0, 0, factor, 0, 0);
	setTransform(m);
	changeResolutionEvent();

}


void SVBMZoomMeshGraphicsView::zoomOut() {

	if(m_zoomLevel  <= -3) return;
	m_zoomLevel = std::max(m_zoomLevel-1, -3000);
	double factor = std::pow(10,m_zoomLevel/20.0);
	QTransform m(factor, 0, 0, factor, 0, 0);
	setTransform(m);
	changeResolutionEvent();

}


void SVBMZoomMeshGraphicsView::setZoomLevel(int zoomLevel) {

	m_zoomLevel = std::max(zoomLevel, -3000);
	m_zoomLevel = std::min(m_zoomLevel, 3000);

	double factor = std::pow(10,m_zoomLevel/20.0);
	QTransform m(factor, 0, 0, factor, 0, 0);
	setTransform(m);
	changeResolutionEvent();

}


void SVBMZoomMeshGraphicsView::resetZoom() {

	m_zoomLevel = 0;
	resetTransform();

}


void SVBMZoomMeshGraphicsView::setGridStep(double gridStep) {
	if (gridStep <=0) return;
	m_gridStep = gridStep;
	viewport()->update();
}


void SVBMZoomMeshGraphicsView::setResolution(double res) {
	if (res <=0) return;
	m_resolution = res;
	viewport()->update();
}

void SVBMZoomMeshGraphicsView::dragEnterEvent(QDragEnterEvent *event){
	event->acceptProposedAction();
}

void SVBMZoomMeshGraphicsView::dragMoveEvent(QDragMoveEvent *event){
	event->acceptProposedAction();
}

void SVBMZoomMeshGraphicsView::dropEvent(QDropEvent *event){
	event->acceptProposedAction();
	QString sentText = event->mimeData()->text();
	QPoint point = event->pos();
	qDebug() << "dropEvent " << sentText << " x: " << QString::number(point.x()) << " y: " << QString::number(point.y());
	if(sentText.startsWith("default")){
		int type = sentText.split(":")[1].toInt();
		addBlock(static_cast<VICUS::NetworkComponent::ModelType>(type), point);
	} else if(sentText.startsWith("db")){
		int type = sentText.split(":")[1].toInt();
		int controllerID = sentText.split(":")[2].toInt();
		addBlock(static_cast<VICUS::NetworkComponent::ModelType>(type), point, controllerID);
	}
}



void SVBMZoomMeshGraphicsView::addBlock(VICUS::BMBlock *block){
	m_sceneManager->addBlock((*block));
}

void SVBMZoomMeshGraphicsView::addBlock(VICUS::NetworkComponent::ModelType type, QPoint point, int controllerID){
	if(controllerID == -1)
		m_sceneManager->addBlock(type, point);
	else
		m_sceneManager->addBlock(type, point, controllerID);
}

void SVBMZoomMeshGraphicsView::removeBlock(){
	m_sceneManager->removeSelectedBlocks();
}


double SVBMZoomMeshGraphicsView::getScaleX(){
	return transform().m11();
}

double SVBMZoomMeshGraphicsView::getScaleY(){
	return transform().m22();
}
