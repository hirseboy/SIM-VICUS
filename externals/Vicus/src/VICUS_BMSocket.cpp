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

#include "VICUS_BMSocket.h"

#include <QXmlStreamWriter>
#include <QStringList>
#include <QDebug>

#include "BM_XMLHelpers.h"

namespace VICUS {


void BMSocket::readXML(QXmlStreamReader & reader) {
    Q_ASSERT(reader.isStartElement());
    // read attributes of Block element
    m_name = reader.attributes().value("name").toString();
    // read child tags
    while (!reader.atEnd() && !reader.hasError()) {
        reader.readNext();
        if (reader.isStartElement()) {
            QString ename = reader.name().toString();
            if (ename == "Position") {
                QString pos = BLOCKMOD::readTextElement(reader);
                m_pos = BLOCKMOD::decodePoint(pos);
            }
            else if (ename == "Orientation") {
                QString orient = BLOCKMOD::readTextElement(reader);
                if (orient == "Horizontal")
                    m_orientation = Qt::Horizontal;
                else
                    m_orientation = Qt::Vertical;
            }
            else if (ename == "Inlet") {
                QString flag = BLOCKMOD::readTextElement(reader);
                m_isInlet = (flag == "true");
            }
            else if(ename == "ID") {
                QString readID = BLOCKMOD::readTextElement(reader);
                m_id = readID.toInt();
            }
            else {
                // unknown element, skip it and all its child elements
                reader.raiseError(QString("Found unknown element '%1' in Socket tag.").arg(ename));
                return;
            }
        }
        else if (reader.isEndElement()) {
            QString ename = reader.name().toString();
            if (ename == "Socket")
                break;// done with XML tag
        }
    }
}


void BMSocket::writeXML(QXmlStreamWriter & writer) const {
    writer.writeStartElement("Socket");
    writer.writeAttribute("name", m_name);
    writer.writeTextElement("Position", BLOCKMOD::encodePoint(m_pos));
    writer.writeTextElement("Orientation", m_orientation == Qt::Horizontal ? "Horizontal" : "Vertical");
    writer.writeTextElement("Inlet", m_isInlet ? "true" : "false");
    writer.writeTextElement("ID", QString::number(m_id));
    writer.writeEndElement();
}


} // namespace VICUS