#pragma once

#include <QLabel>
#include <QMouseEvent>

class ThumbnailWidget  : public QLabel
{
	Q_OBJECT

public:
	explicit ThumbnailWidget(int index, QWidget *parent = nullptr);
	~ThumbnailWidget();

	void setSelected(bool selected);
	bool isSelected() const { return m_selected; }
	int getIndex() const { return m_index; }

signals:
	void clicked(int index, Qt::KeyboardModifiers modifiers);
	void doubleClicked(int index);

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
	int m_index;
	bool m_selected;
};
