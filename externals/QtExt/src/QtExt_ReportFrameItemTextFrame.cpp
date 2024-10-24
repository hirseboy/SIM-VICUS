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

#include "QtExt_ReportFrameItemTextFrame.h"

namespace QtExt {

ReportFrameItemTextFrame::ReportFrameItemTextFrame(TextFrame* textFrame, QPaintDevice* paintDevice, double width, double spaceAfter, double spaceBefore, bool canPageBreakAfter) :
	ReportFrameItemBase(paintDevice, width, spaceAfter, spaceBefore, canPageBreakAfter),
	m_textFrame(textFrame)
{
}

void ReportFrameItemTextFrame::setCurrentRect() {
	m_currentRect = m_textFrame->frameRect(m_paintDevice, m_width);
}

void ReportFrameItemTextFrame::drawItem(QPainter* painter, QPointF& pos) {
	m_textFrame->drawFrame(painter, pos, m_width);
}

//ReportFrameItemBase* ReportFrameItemTextFrame::clone() const {
//	ReportFrameItemTextFrame* res = new ReportFrameItemTextFrame(*this);
//	return res;
//}


} // namespace QtExt
