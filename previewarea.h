#pragma once

#include <QScrollArea>
#include <QWidget>
#include <QGridLayout>
#include <QVector>
#include <QString>
#include <QPixmap>
#include "ThumbnailWidget.h"

class PreviewArea  : public QScrollArea
{
	Q_OBJECT

public:
	explicit PreviewArea(QWidget *parent);
	~PreviewArea();

	// Основные методы
	void setThumbnailSize(int size);
	void setColumns(int columns);
	void setThumbnailCount(int count);
	void clearThumbnails();
	void addThumbnail(int index, const QString& filename, const QPixmap& pixmap);
	void setPlaceholder(int index, const QString& text = "Loading...");
	void setSelection(const QSet<int>& indices);

	// Управление выделением
	void clearSelection();
	QSet<int> getSelectedIndices() const { return selectedIndices; }
	QStringList getSelectedFilenames() const;

	// Получение данных
	int getThumbnailCount() const { return thumbnailWidgets.size(); }

signals:
	// Сигналы для внешнего мира
	void thumbnailClicked(int index, Qt::KeyboardModifiers modifiers);
	void thumbnailDoubleClicked(int index);
	void selectionCleared();
	void selectionChanged(const QSet<int>& selectedIndices);

public slots:
	void onThumbnailLoaded(int index, const QPixmap& pixmap);
	void onThumbnailWidgetDoubleClicked(int index);
	void updateSelection(int index, Qt::KeyboardModifiers modifiers);
protected:
	bool eventFilter(QObject *obj, QEvent *event) override;
private:
	// UI элементы
	QWidget *container;
	QGridLayout *layout;
	QVector<ThumbnailWidget*> thumbnailWidgets;
	QVector<QString> filenames;

	// Настройки
	int thumbnailSize;
	int columns;

	// Выделение
	QSet<int> selectedIndices;
	int lastSelectedIndex;

	// Фоновая подсветка
	void updateBackgroundStyle();
	bool hasSelection() const { return !selectedIndices.isEmpty(); }

private slots:
	void onThumbnailWidgetClicked(int index, Qt::KeyboardModifiers modifiers);
	void onContainerClicked();
};
