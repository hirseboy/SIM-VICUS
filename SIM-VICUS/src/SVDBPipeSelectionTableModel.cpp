/*	SIM-VICUS - Building and District Energy Simulation Tool.

	Copyright (c) 2020-today, Institut für Bauklimatik, TU Dresden, Germany

	Primary authors:
	  Andreas Nicolai  <andreas.nicolai -[at]- tu-dresden.de>
	  Dirk Weiss  <dirk.weiss -[at]- tu-dresden.de>
	  Stephan Hirth  <stephan.hirth -[at]- tu-dresden.de>
	  Hauke Hirsch  <hauke.hirsch -[at]- tu-dresden.de>

	  ... all the others from the SIM-VICUS team ... :-)

	This program is part of SIM-VICUS (https://github.com/ghorwin/SIM-VICUS)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
*/

#include "SVDBPipeSelectionTableModel.h"

#include <QIcon>
#include <QFont>
#include <QTableView>
#include <QHeaderView>

#include <VICUS_NetworkPipe.h>
#include <VICUS_Database.h>
#include <VICUS_KeywordListQt.h>

#include <QtExt_LanguageHandler.h>
#include <SVConversions.h>

#include <NANDRAD_KeywordList.h>

#include "SVConstants.h"

SVDBPipeSelectionTableModel::SVDBPipeSelectionTableModel(QObject *parent, SVDatabase &db) :
	SVAbstractDatabaseTableModel(parent),
	m_db(&db)
{
	Q_ASSERT(m_db != nullptr);
}

int SVDBPipeSelectionTableModel::columnCount(const QModelIndex &) const
{
	return NumColumns;
}


QVariant SVDBPipeSelectionTableModel::data( const QModelIndex & index, int role) const {
	if (!index.isValid())
		return QVariant();

	// readability improvement
	const VICUS::Database<VICUS::NetworkPipe> & pipes = m_db->m_pipes;

	int row = index.row();
	if (row >= static_cast<int>(pipes.size()))
		return QVariant();

	std::map<unsigned int, VICUS::NetworkPipe>::const_iterator it = pipes.begin();
	std::advance(it, row);

	switch (role) {
		case Qt::DisplayRole : {
			// Note: when accessing multilanguage strings below, take name in current language or if missing, "all"
			switch (index.column()) {
				case ColId					: return it->first;
				case ColName				: return QtExt::MultiLangString2QString(it->second.m_displayName);
				case ColCategory			: return QtExt::MultiLangString2QString(it->second.m_categoryName);
				case ColManufacturer		: {
					QString name = it->second.m_manufacturerName;
					if (name.size()==0)
						return tr("generic");
					else
						return name;
				}
				case ColProduct				: return it->second.m_productName;
				case ColSDR					: return it->second.SDRvalue();
			}
		} break;

		case Qt::DecorationRole : {
			if (index.column() == ColCheck) {
				if (it->second.isValid())
					return QIcon(":/gfx/actions/16x16/ok.png");
				else
					return QIcon(":/gfx/actions/16x16/error.png");
			}
		} break;

		case Qt::BackgroundRole : {
			if (index.column() == ColColor) {
				return it->second.m_color;
			}
		} break;

		case Qt::SizeHintRole :
			switch (index.column()) {
				case ColCheck :
					return QSize(16, 16);
				case ColColor :
					return QSize(22, 16);
				case ColSelection :
					return QSize(20, 20);
			} // switch
			break;

		case Role_Id :
			return it->first;

		case Role_BuiltIn :
			return it->second.m_builtIn;

		case Role_Local :
			return it->second.m_local;

		case Role_Referenced:
			return it->second.m_isReferenced;

		case Role_PreselectedForProject: {
			return it->second.m_selected;
		}

		case Qt::ToolTipRole: {
			if(index.column() == ColCheck) {
				std::string errorMsg = "";
				if (!it->second.isValid())
					return QString::fromStdString(it->second.m_errorMsg);
			}
		}
	}

	return QVariant();
}


int SVDBPipeSelectionTableModel::rowCount ( const QModelIndex & ) const {
	return (int)m_db->m_pipes.size();
}


QVariant SVDBPipeSelectionTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation == Qt::Vertical)
		return QVariant();

	switch (role) {
		case Qt::DisplayRole: {
			switch ( section ) {
				case ColId					: return tr("Id");
				case ColName				: return tr("Name");
				case ColCategory			: return tr("Category");
				case ColManufacturer		: return tr("Manufacturer");
				case ColProduct				: return tr("Product");
				case ColSDR					: return tr("SDR");
			}
		} break;

		case Qt::FontRole : {
			QFont f;
			f.setBold(true);
			f.setPointSizeF(f.pointSizeF()*0.8);
			return f;
		}
	} // switch
	return QVariant();
}


void SVDBPipeSelectionTableModel::resetModel() {
	beginResetModel();
	endResetModel();
}


QModelIndex SVDBPipeSelectionTableModel::addNewItem() {
	VICUS::NetworkPipe pipe;

	// Mind: use the values in units as defined in the keyword list!

	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_DiameterOutside, 25);
	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_ThicknessWall, 2.3);
	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_RoughnessWall, 7e-3);
	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_ThermalConductivityWall, 0.4);
	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_ThicknessInsulation, 0);
	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_ThermalConductivityInsulation, 0.035);
	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_HeatCapacityWall, 1900);
	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_DensityWall, 960);
	VICUS::KeywordList::setParameter(pipe.m_para, "NetworkPipe::para_t", VICUS::NetworkPipe::P_ThicknessOuterLayer, 0);

	pipe.m_categoryName.setString("PE", QtExt::LanguageHandler::instance().langId().toStdString());
	pipe.m_displayName = pipe.nameFromData();

	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	unsigned int id = m_db->m_pipes.add( pipe );
	endInsertRows();
	QModelIndex idx = indexById(id);
	return idx;
}


QModelIndex SVDBPipeSelectionTableModel::copyItem(const QModelIndex & existingItemIndex) {
	// lookup existing item
	const VICUS::Database<VICUS::NetworkPipe> & db = m_db->m_pipes;
	Q_ASSERT(existingItemIndex.isValid() && existingItemIndex.row() < (int)db.size());
	std::map<unsigned int, VICUS::NetworkPipe>::const_iterator it = db.begin();
	std::advance(it, existingItemIndex.row());
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	// create new item and insert into DB
	VICUS::NetworkPipe newItem(it->second);
	unsigned int id = m_db->m_pipes.add( newItem );
	endInsertRows();
	QModelIndex idx = indexById(id);
	return idx;
}



void SVDBPipeSelectionTableModel::deleteItem(const QModelIndex & index) {
	if (!index.isValid())
		return;
	unsigned int id = data(index, Role_Id).toUInt();
	beginRemoveRows(QModelIndex(), index.row(), index.row());
	m_db->m_pipes.remove(id);
	endRemoveRows();
}


void SVDBPipeSelectionTableModel::setColumnResizeModes(QTableView * tableView) {
	tableView->horizontalHeader()->setSectionResizeMode(SVDBPipeSelectionTableModel::ColId, QHeaderView::ResizeToContents);
	tableView->horizontalHeader()->setSectionResizeMode(SVDBPipeSelectionTableModel::ColCheck, QHeaderView::ResizeToContents);
	tableView->horizontalHeader()->setSectionResizeMode(SVDBPipeSelectionTableModel::ColSelection, QHeaderView::Fixed);
	tableView->horizontalHeader()->setSectionResizeMode(SVDBPipeSelectionTableModel::ColManufacturer, QHeaderView::ResizeToContents);
	tableView->horizontalHeader()->setSectionResizeMode(SVDBPipeSelectionTableModel::ColProduct, QHeaderView::ResizeToContents);
	tableView->horizontalHeader()->setSectionResizeMode(SVDBPipeSelectionTableModel::ColName, QHeaderView::Stretch);
	tableView->horizontalHeader()->setSectionResizeMode(SVDBPipeSelectionTableModel::ColSDR, QHeaderView::Fixed);
	tableView->horizontalHeader()->setSectionResizeMode(SVDBPipeSelectionTableModel::ColCategory, QHeaderView::Fixed);
}


void SVDBPipeSelectionTableModel::setItemLocal(const QModelIndex &index, bool local)
{
	if (!index.isValid())
		return;
	unsigned int id = data(index, Role_Id).toUInt();
	m_db->m_pipes[id]->m_local = local;
	m_db->m_pipes.m_modified = true;
	setItemModified(id);
}


void SVDBPipeSelectionTableModel::setItemModified(unsigned int id) {
	QModelIndex idx = indexById(id);
	QModelIndex left = index(idx.row(), 0);
	QModelIndex right = index(idx.row(), NumColumns-1);
	emit dataChanged(left, right);
}


QModelIndex SVDBPipeSelectionTableModel::indexById(unsigned int id) const {
	for (int i=0; i<rowCount(); ++i) {
		QModelIndex idx = index(i, 0);
		if (data(idx, Role_Id).toUInt() == id)
			return idx;
	}
	return QModelIndex();
}