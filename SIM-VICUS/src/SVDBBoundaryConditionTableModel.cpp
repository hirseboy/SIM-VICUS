#include "SVDBBoundaryConditionTableModel.h"

#include <QIcon>
#include <QFont>

#include <VICUS_BoundaryCondition.h>
#include <VICUS_Database.h>
#include <VICUS_KeywordListQt.h>

#include <NANDRAD_KeywordList.h>

#include <QtExt_LanguageHandler.h>

#include "SVConstants.h"
#include "SVDBBoundaryConditionEditDialog.h"


SVDBBoundaryConditionTableModel::SVDBBoundaryConditionTableModel(QObject *parent, SVDatabase &db) :
	QAbstractTableModel(parent),
	m_db(&db)
{
	// must only be created from SVDBBoundaryConditionEditDialog
	Q_ASSERT(dynamic_cast<SVDBBoundaryConditionEditDialog*>(parent) != nullptr);
	Q_ASSERT(m_db != nullptr);
}

SVDBBoundaryConditionTableModel::~SVDBBoundaryConditionTableModel() {
}


int SVDBBoundaryConditionTableModel::columnCount ( const QModelIndex & ) const {
	return NumColumns;
}


QVariant SVDBBoundaryConditionTableModel::data ( const QModelIndex & index, int role) const {
	if (!index.isValid())
		return QVariant();

	// readability improvement
	const VICUS::Database<VICUS::BoundaryCondition> & bcDB = m_db->m_boundaryConditions;

	int row = index.row();
	if (row >= (int)bcDB.size())
		return QVariant();

	std::map<unsigned int, VICUS::BoundaryCondition>::const_iterator it = bcDB.begin();
	std::advance(it, row);

	switch (role) {
		case Qt::DisplayRole : {
			// Note: when accessing multilanguage strings below, take name in current language or if missing, "all"
			std::string langId = QtExt::LanguageHandler::instance().langId().toStdString();
			std::string fallBackLangId = "en";

			switch (index.column()) {
				case ColId					: return it->first;
				case ColName				: return QString::fromStdString(it->second.m_displayName.string(langId, fallBackLangId));
			}
		} break;

		case Qt::SizeHintRole :
			switch (index.column()) {
				case ColCheck :
					return QSize(22, 16);
			} // switch
			break;

		case Role_Id :
			return it->first;

		case Role_BuiltIn :
			return it->second.m_builtIn;
	}

	return QVariant();
}


int SVDBBoundaryConditionTableModel::rowCount ( const QModelIndex & ) const {
	return (int)m_db->m_boundaryConditions.size();
}


QVariant SVDBBoundaryConditionTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation == Qt::Vertical)
		return QVariant();
	switch (role) {
		case Qt::DisplayRole: {
			switch ( section ) {
				case ColId					: return tr("Id");
				case ColName				: return tr("Name");
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


QModelIndex SVDBBoundaryConditionTableModel::addNewItem() {
	VICUS::BoundaryCondition bc;
	bc.m_displayName.setEncodedString("en:<new boundary condition>");

	//set default parameters
	bc.m_heatConduction.m_modelType = NANDRAD::InterfaceHeatConduction::MT_Constant;
	NANDRAD::KeywordList::setParameter(bc.m_heatConduction.m_para, "InterfaceHeatConduction::para_t", NANDRAD::InterfaceHeatConduction::P_HeatTransferCoefficient, 8);

	bc.m_solarAbsorption.m_modelType = NANDRAD::InterfaceSolarAbsorption::MT_Constant;
	NANDRAD::KeywordList::setParameter(bc.m_solarAbsorption.m_para, "InterfaceSolarAbsorption::para_t", NANDRAD::InterfaceSolarAbsorption::P_AbsorptionCoefficient, 0.6);

	bc.m_longWaveEmission.m_modelType = NANDRAD::InterfaceLongWaveEmission::MT_Constant;
	NANDRAD::KeywordList::setParameter(bc.m_longWaveEmission.m_para, "InterfaceLongWaveEmission::para_t", NANDRAD::InterfaceLongWaveEmission::P_Emissivity, 0.9);

	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	unsigned int id = m_db->m_boundaryConditions.add( bc );
	endInsertRows();
	QModelIndex idx = indexById(id);
	return idx;
}


QModelIndex SVDBBoundaryConditionTableModel::addNewItem(VICUS::BoundaryCondition bc) {
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	unsigned int id = m_db->m_boundaryConditions.add( bc );
	endInsertRows();
	QModelIndex idx = indexById(id);
	return idx;
}


bool SVDBBoundaryConditionTableModel::deleteItem(QModelIndex index) {
	if (!index.isValid())
		return false;
	unsigned int id = data(index, Role_Id).toUInt();
	beginRemoveRows(QModelIndex(), index.row(), index.row());
	m_db->m_boundaryConditions.remove(id);
	endRemoveRows();
	return true;
}


void SVDBBoundaryConditionTableModel::resetModel() {
	beginResetModel();
	endResetModel();
}


void SVDBBoundaryConditionTableModel::setItemModified(unsigned int id) {
	QModelIndex idx = indexById(id);
	QModelIndex left = index(idx.row(), 0);
	QModelIndex right = index(idx.row(), NumColumns-1);
	emit dataChanged(left, right);
}


QModelIndex SVDBBoundaryConditionTableModel::indexById(unsigned int id) const {
	for (int i=0; i<rowCount(); ++i) {
		QModelIndex idx = index(i, 0);
		if (data(idx, Role_Id).toUInt() == id)
			return idx;
	}
	return QModelIndex();
}
