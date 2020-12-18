#ifndef SVDBCONSTRUCTIONEDITDIALOG_H
#define SVDBCONSTRUCTIONEDITDIALOG_H

#include <QDialog>

namespace Ui {
	class SVDBConstructionEditDialog;
}

class SVDBConstructionEditWidget;

/*! The edit dialog for construction types. */
class SVDBConstructionEditDialog : public QDialog {
	Q_OBJECT

public:
	explicit SVDBConstructionEditDialog(QWidget *parent = nullptr);
	~SVDBConstructionEditDialog();

	/*! Starts the dialog in "edit constructions" mode. */
	void edit();

	/*! Starts the dialog in "select construction mode".
		\return If a construction was selected and double-clicked/or the "Select" button was
				pressed, the function returns the ID of the selected construction. Otherwise, if the
				dialog was aborted, the function returns 0.
	*/
	unsigned int select();

private slots:
	void on_pushButtonSelect_clicked();

	void on_pushButtonCancel_clicked();

	void on_pushButtonClose_clicked();

private:
	Ui::SVDBConstructionEditDialog	*m_ui;

	/*! The actual edit widget for a single construction type. */
	SVDBConstructionEditWidget		*m_constructionEditWidget = nullptr;
};

#endif // SVDBCONSTRUCTIONEDITDIALOG_H
