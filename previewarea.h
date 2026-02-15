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

	void clearThumbnails();

	// Основные методы
	void setThumbnailSize(int size);
	void setTotalCount(int count);

	void removeFiles(const QList<int>& indices);
	void removeFile(int index);
			
	// Добавляем новый метод для установки превью
	void setThumbnail(int index, const QPixmap& pixmap);
	void setFilename(int index, const QString& filename);
	
	// Управление выделением
	void setSelection(const QSet<int>& indices);
	void clearSelection();
	QSet<int> getSelectedIndices() const { return selectedIndices; }
	QStringList getSelectedFilenames() const;

	// Геттеры
	int getTotalCount() const { return totalCount; }
	int getThumbnailSize() const { return thumbnailSize; }
	
signals:
	// Сигналы для внешнего мира
	void thumbnailClicked(int index, Qt::KeyboardModifiers modifiers);
	void thumbnailDoubleClicked(int index);
	void selectionCleared();
	void selectionChanged(const QSet<int>& selectedIndices);
	void visibleRangeChanged(int first, int last);

public slots:
	void onThumbnailLoaded(int index, const QPixmap& pixmap);

protected:
	void resizeEvent(QResizeEvent *event) override;
	void scrollContentsBy(int dx, int dy) override; // Важно для виртуализации
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
private:
	// Виртуальные индексы
	int totalCount;
	int firstVisibleIndex;
	int lastVisibleIndex;

	// Реальные виджеты (только видимые)
	QVector<ThumbnailWidget*> visibleWidgets;  // виджеты в порядке отображения

	// Данные для всех элементов
	QVector<QString> filenames;		// имена всех файлов (индекс -> имя)
	QVector<QPixmap> thumbnails;    // превью всех файлов (индекс -> картинка)

	// UI элементы
	QWidget *container;
	QTimer *scrollTimer;
	
	// Настройки
	int currentColumns;
	int thumbnailSize;
	int spacing;

	// Выделение
	QSet<int> selectedIndices;
	int lastSelectedIndex;

	// Вспомогательные методы
private:
	ThumbnailWidget* createThumbnailWidget(int index);
	ThumbnailWidget* getOrCreateWidget(int index);
	QRect getWidgetGeometry(int index) const;
	int indexAt(const QPoint& pos) const;	
	void updateContainerSize();
	void updateColumns();
	void recreateVisibleWidgets();        // полное пересоздание видимых виджетов
	void shiftIndicesAfterRemoval(int removedCount);  // сдвиг индексов после удаления
	void updateScrollStep();
	void updateScrollBarRange();
	// Фоновая подсветка
	void updateBackgroundStyle();
	bool hasSelection() const { return !selectedIndices.isEmpty(); }

private slots:
	void onThumbnailWidgetClicked(int index, Qt::KeyboardModifiers modifiers);
	void onThumbnailWidgetDoubleClicked(int index);
	void updateVisibleRange();
};
