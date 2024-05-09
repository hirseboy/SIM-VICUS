#include "SVSubNetworkEditDialogTableItem.h"
#include "ui_SVSubNetworkEditDialogTableItem.h"

#include "SVConstants.h"
#include "SVStyle.h"

#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QLabel>
#include <QHBoxLayout>


SVSubNetworkEditDialogTableItem::SVSubNetworkEditDialogTableItem(QString filename, QString text, QString tooltip, int height, QWidget *parent, bool subCategory, bool inBuild) :
	QWidget(parent),
	m_ui(new Ui::SVSubNetworkEditDialogTableItem),
	m_inbuild(inBuild)
{
	m_ui->setupUi(this);
	if(!subCategory){
		QLabel *iconLabel = new QLabel(this);
		QLabel *textLabel = new QLabel(text, this);
		QHBoxLayout *layout = new QHBoxLayout(this);

		QSvgRenderer renderer(filename);
		QPixmap pixmap(height, height);
		pixmap.fill(Qt::transparent);

		QPainter painter(&pixmap);
		renderer.render(&painter);
		painter.end();

		iconLabel->setPixmap(pixmap);
		iconLabel->setMinimumSize(height, height);
		iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		textLabel->setMinimumSize(0, 0);
		textLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		textLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		layout->addWidget(iconLabel);
		layout->addWidget(textLabel);
		layout->setContentsMargins(0,0,0,0);
		layout->setSpacing(10);
		layout->setStretchFactor(iconLabel, 0);
		layout->setStretchFactor(textLabel, 1);
		setToolTip(tooltip);
		setLayout(layout);
	} else {
		QLabel *textLabel = new QLabel(text, this);
		textLabel->setMinimumSize(0, 0);
		textLabel->setAlignment(Qt::AlignLeft| Qt::AlignVCenter); // This will center the text both horizontally and vertically
		textLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

		QHBoxLayout *layout = new QHBoxLayout(this);
		layout->addSpacing(height + 20);
		layout->addWidget(textLabel);
		layout->setContentsMargins(0,0,0,0);
		layout->setStretchFactor(textLabel, 0);
		setToolTip(tooltip);
		setLayout(layout);
	}

	setToolTip(QString("<p style='color: #000000; background-color: #ffffff; border: 1px solid black;'>%1</p>").arg(tooltip));

	this->setAutoFillBackground(true);

}

SVSubNetworkEditDialogTableItem::~SVSubNetworkEditDialogTableItem()
{
	delete m_ui;
}
