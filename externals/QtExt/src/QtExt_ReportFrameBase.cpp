/*	QtExt - Qt-based utility classes and functions (extends Qt library)

	Copyright (c) 2014-today, Institut für Bauklimatik, TU Dresden, Germany

	Primary authors:
	  Heiko Fechner    <heiko.fechner -[at]- tu-dresden.de>
	  Andreas Nicolai

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Dieses Programm ist Freie Software: Sie können es unter den Bedingungen
	der GNU General Public License, wie von der Free Software Foundation,
	Version 3 der Lizenz oder (nach Ihrer Wahl) jeder neueren
	veröffentlichten Version, weiter verteilen und/oder modifizieren.

	Dieses Programm wird in der Hoffnung bereitgestellt, dass es nützlich sein wird, jedoch
	OHNE JEDE GEWÄHR,; sogar ohne die implizite
	Gewähr der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
	Siehe die GNU General Public License für weitere Einzelheiten.

	Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
	Programm erhalten haben. Wenn nicht, siehe <https://www.gnu.org/licenses/>.
*/

#include "QtExt_ReportFrameBase.h"

#include "QtExt_Report.h"
#include "QtExt_TextProperties.h"

namespace QtExt {

ReportFrameBase::ReportFrameBase(	Report* report, QTextDocument* textDocument ) :
	m_onNewPage(false),
	m_isHidden(false),
	m_report(report),
	m_textDocument(textDocument)
{
}

void ReportFrameBase::update(QPaintDevice* , double width) {
	qreal height = 0;
	for(auto& item : m_items) {
		if(item->isVisible())
			height += item->rect().height();
	}
	m_wholeFrameRect = QRectF(0, 0, width, height);
}

void ReportFrameBase::print(QPainter * p, const QRectF & frame) {
	QPointF pos = frame.topLeft();
	for(auto& item : m_items) {
		if(item->isVisible())
			item->draw(p, pos);
	}
}

size_t ReportFrameBase::addItem(ReportFrameItemBase* item) {
	m_items.push_back(std::unique_ptr<ReportFrameItemBase>(item));
	return m_items.size() - 1;
}

ReportFrameItemBase* ReportFrameBase::item(size_t index) {
	Q_ASSERT(index < m_items.size());
	return m_items[index].get();
}

} // namespace QtExt {
