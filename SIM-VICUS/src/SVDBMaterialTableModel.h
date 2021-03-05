#ifndef SVDBMaterialTableModelH
#define SVDBMaterialTableModelH

#include "SVAbstractDatabaseEditWidget.h"

#include "SVDatabase.h"

/*! Model for accessing the materials in the materials database.

	The individual columns have different values for the DisplayRole (and some
	also for alignment). The column 'ColCheck' shows an indication if all data
	in construction and used materials is valid.
	All columns (i.e. all model indexes) return custom role data for global
	id and built-in roles (see SVConstants.h).
*/
class SVDBMaterialTableModel : public SVAbstractDatabaseTableModel {
	Q_OBJECT
public:

	/*! Columns shown in the table view. */
	enum Columns {
		ColId,
		ColCheck,
		ColName,
		ColCategory,
		ColLambda,
		ColRho,
		ColCet,
		ColProductID,
		ColProducer,
		ColSource,
//		ColMew,
//		ColW80,
//		ColWSat,
//		ColAw,
//		ColSd,
//		ColKAir,
		NumColumns
	};


	/*! Constructor, requires a read/write pointer to the central database object.
		\note Pointer to database must be valid throughout the lifetime of the Model!
	*/
	SVDBMaterialTableModel(QObject * parent, SVDatabase & db);

	// ** QAbstractItemModel interface **

	virtual int columnCount ( const QModelIndex & ) const override { return NumColumns; }
	virtual QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const override;
	virtual int rowCount ( const QModelIndex & parent = QModelIndex() ) const override;
	virtual QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;

	// ** SVAbstractDatabaseTableModel interface **

	int columnIndexId() const override { return ColId; }
	SVDatabase::DatabaseTypes databaseType() const override { return SVDatabase::DT_Materials; }
	virtual void resetModel() override;
	QModelIndex addNewItem() override;
	QModelIndex copyItem(const QModelIndex & index) override;
	void deleteItem(const QModelIndex & index) override;
	void setColumnResizeModes(QTableView * tableView) override;

	// ** other members **

	/*! Tells the model that an item has been modified, triggers a dataChanged() signal. */
	void setItemModified(unsigned int id);

private:
	/*! Returns an index for a given Id. */
	QModelIndex indexById(unsigned int id) const;

	/*! Pointer to the entire database (not owned). */
	SVDatabase	* m_db;
};

#endif // SVDBMaterialTableModelH
