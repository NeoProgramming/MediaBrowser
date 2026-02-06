#include "thumbnailwidget.h"
#include <QPainter>

ThumbnailWidget::ThumbnailWidget(int index, QWidget *parent)
	: QLabel(parent)
	, m_index(index)
	, m_selected(false)
{
	setAlignment(Qt::AlignCenter);
	setCursor(Qt::PointingHandCursor);
}

ThumbnailWidget::~ThumbnailWidget()
{
}

void ThumbnailWidget::setSelected(bool selected)
{
	if (m_selected != selected) {
		m_selected = selected;

		// Обновляем стиль
		if (m_selected) {
			setStyleSheet("border: 3px solid #2196F3; background: #e3f2fd; padding: 2px;");
		}
		else {
			setStyleSheet("border: 1px solid #ccc; background: #f0f0f0; padding: 2px;");
		}

		update();
	}
}

void ThumbnailWidget::mousePressEvent(QMouseEvent *event)
{
	QLabel::mousePressEvent(event);

	if (event->button() == Qt::LeftButton) {
		emit clicked(m_index, event->modifiers());
	}
}

void ThumbnailWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
	QLabel::mouseDoubleClickEvent(event);

	if (event->button() == Qt::LeftButton) {
		emit doubleClicked(m_index);
	}
}
