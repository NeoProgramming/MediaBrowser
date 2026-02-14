#include "previewarea.h"
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>
#include <QGridLayout>
#include <QTimer>
#include <QScrollBar>

PreviewArea::PreviewArea(QWidget *parent)
	: QScrollArea(parent)
	, totalCount(0)
	, firstVisibleIndex(-1)
	, lastVisibleIndex(-1)
	, thumbnailSize(200)
	, spacing(10)
	, lastSelectedIndex(-1)
	, container(nullptr)
	, scrollTimer(nullptr)
	, currentColumns(4)
{
	// Настройка области прокрутки
	setWidgetResizable(true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	// Создаем контейнер
	container = new QWidget();
	container->setAttribute(Qt::WA_TransparentForMouseEvents); // Важно!
	
	// Делаем контейнер обрабатывающим события
	container->setMouseTracking(true);
	container->installEventFilter(this);

	// Стиль фона
	container->setStyleSheet(
		"QWidget {"
		"    background-color: #fafafa;"
		"    border: 1px dashed #e0e0e0;"
		"}"
	);
		
	setWidget(container);

	// Устанавливаем политику фокуса для получения событий клавиатуры
	setFocusPolicy(Qt::StrongFocus);

	updateBackgroundStyle();

	// Таймер для сглаживания скролла
	scrollTimer = new QTimer(this);
	scrollTimer->setSingleShot(true);
	scrollTimer->setInterval(50);
	connect(scrollTimer, &QTimer::timeout, this, &PreviewArea::updateVisibleRange);
	connect(verticalScrollBar(), &QScrollBar::valueChanged,
		[this](int) { scrollTimer->start(); });
}

PreviewArea::~PreviewArea()
{
	clearThumbnails();
	if (container) {
		container->removeEventFilter(this);
	}
}

void PreviewArea::setThumbnailSize(int size)
{
	thumbnailSize = size;
	updateColumns();
	updateContainerSize();
	updateVisibleRange();
}

void PreviewArea::updateColumns()
{
	if (!viewport()) return;

	int availableWidth = viewport()->width() - spacing;  // Учитываем отступы
	if (availableWidth <= 0) return;

	// Рассчитываем максимальное количество колонок
	int newColumns = availableWidth / (thumbnailSize + spacing);
	newColumns = qMax(1, newColumns);  // Минимум 1 колонка

	if (newColumns != currentColumns) {
		
		currentColumns = newColumns;

		// ВАЖНО: При изменении количества колонок нужно:
		// 1. Обновить размер контейнера
		updateContainerSize();

		// 2. Полностью пересоздать видимые виджеты
		// Очищаем кэш видимых индексов, чтобы принудительно пересоздать все
		destroyWidgetsOutsideRange(-1, -1);  // Удаляем ВСЕ виджеты
		firstVisibleIndex = -1;
		lastVisibleIndex = -1;

		// 3. Заново определить видимый диапазон и создать виджеты
		updateVisibleRange();

		qDebug() << "Columns changed to:" << currentColumns;

		// Можно добавить сигнал, если нужно оповещать другие компоненты
		// emit columnsChanged(currentColumns);
	}
}

void PreviewArea::setTotalCount(int count)
{
	totalCount = count;
	filenames.resize(count);
	updateContainerSize();
	updateVisibleRange();
}

void PreviewArea::clearThumbnails()
{
	// Удаляем все виджеты
	for (auto widget : visibleWidgets) {
		widget->deleteLater();
	}
	visibleWidgets.clear();
	thumbnailCache.clear();
	filenames.clear();
	selectedIndices.clear();
	totalCount = 0;
	firstVisibleIndex = -1;
	lastVisibleIndex = -1;
}


void PreviewArea::updateVisibleRange()
{
	if (totalCount == 0) return;

	QRect visibleRect = viewport()->rect();
	visibleRect.translate(0, verticalScrollBar()->value());

	int itemHeight = thumbnailSize + spacing;

	// Более точный расчет видимых строк
	int firstRow = visibleRect.top() / itemHeight;
	// Если верхняя граница находится внутри строки, все равно включаем эту строку
	if (visibleRect.top() % itemHeight != 0 && firstRow > 0) {
		// Оставляем как есть - строка уже включена делением
	}

	int lastRow = (visibleRect.bottom() + itemHeight - 1) / itemHeight; // Важно: округление вверх
	// Корректируем, чтобы не выйти за пределы
	lastRow = qMin(lastRow, (totalCount - 1) / currentColumns);

	int newFirst = qMax(0, firstRow * currentColumns);
	int newLast = qMin(totalCount - 1, (lastRow + 1) * currentColumns - 1);

	// Добавляем буферные строки (увеличим до 3 для надежности)
	int bufferRows = 3;
	newFirst = qMax(0, newFirst - bufferRows * currentColumns);
	newLast = qMin(totalCount - 1, newLast + bufferRows * currentColumns);

	// Отладка
	// qDebug() << "Visible rows:" << firstRow << "-" << lastRow
	//          << "Indices:" << newFirst << "-" << newLast;

	if (newFirst != firstVisibleIndex || newLast != lastVisibleIndex) {
		destroyWidgetsOutsideRange(newFirst, newLast);
		createWidgetsForRange(newFirst, newLast);

		firstVisibleIndex = newFirst;
		lastVisibleIndex = newLast;

		emit visibleRangeChanged(firstVisibleIndex, lastVisibleIndex);
	}
}

void PreviewArea::createWidgetsForRange(int first, int last)
{
	for (int i = first; i <= last; ++i) {
		if (!visibleWidgets.contains(i)) {
			ThumbnailWidget *widget = new ThumbnailWidget(i, container);
			widget->setFixedSize(thumbnailSize, thumbnailSize);

			// Устанавливаем данные, если есть
			if (!filenames[i].isEmpty()) {
				widget->setText(filenames[i]);
			}

			if (thumbnailCache.contains(i)) {
				widget->setPixmap(thumbnailCache[i]);
			}

			if (selectedIndices.contains(i)) {
				widget->setSelected(true);
			}

			// Позиционируем
			int row = i / currentColumns;
			int col = i % currentColumns;
			int x = col * (thumbnailSize + 10) + 5;
			int y = row * (thumbnailSize + 10) + 5;
			widget->setGeometry(x, y, thumbnailSize, thumbnailSize);

			// Подключаем сигналы
			connect(widget, &ThumbnailWidget::clicked,
				this, &PreviewArea::onThumbnailWidgetClicked);
			connect(widget, &ThumbnailWidget::doubleClicked,
				this, &PreviewArea::onThumbnailWidgetDoubleClicked);

			widget->show();
			visibleWidgets[i] = widget;
		}
	}
}

void PreviewArea::destroyWidgetsOutsideRange(int first, int last)
{
	QList<int> toRemove;
	for (auto it = visibleWidgets.begin(); it != visibleWidgets.end(); ++it) {
		if (it.key() < first || it.key() > last) {
			it.value()->deleteLater();
			toRemove.append(it.key());
		}
	}

	for (int index : toRemove) {
		visibleWidgets.remove(index);
	}
}

ThumbnailWidget* PreviewArea::getOrCreateWidget(int index)
{
	ThumbnailWidget* widget = visibleWidgets.value(index, nullptr);
	if (!widget) {
		widget = new ThumbnailWidget(index, this);
		widget->setFixedSize(thumbnailSize, thumbnailSize);

		// Подключаем сигналы
		connect(widget, &ThumbnailWidget::clicked,
			this, &PreviewArea::onThumbnailWidgetClicked);
		connect(widget, &ThumbnailWidget::doubleClicked,
			this, &PreviewArea::onThumbnailWidgetDoubleClicked);
	}
	return widget;
}

QRect PreviewArea::getWidgetGeometry(int index) const
{
	int row = index / currentColumns;
	int col = index % currentColumns;
	int x = col * (thumbnailSize + spacing) + spacing / 2;
	int y = row * (thumbnailSize + spacing) + spacing / 2;
	return QRect(x, y, thumbnailSize, thumbnailSize);
}

void PreviewArea::setThumbnail(int index, const QPixmap& pixmap)
{
	if (index < 0 || index >= totalCount) return;

	// Сохраняем в кэш
	thumbnailCache[index] = pixmap;

	// Если виджет существует, обновляем его
	ThumbnailWidget* widget = visibleWidgets.value(index, nullptr);
	if (widget) {
		widget->setPixmap(pixmap);
		widget->setText("");
	}
}

void PreviewArea::setFilename(int index, const QString& filename)
{
	if (index < 0 || index >= totalCount) return;
	filenames[index] = filename;
	
	// Если виджет существует, обновляем его текст
	ThumbnailWidget* widget = visibleWidgets.value(index, nullptr);
	if (widget && !thumbnailCache.contains(index)) {
		widget->setText(filename);
	}
}

int PreviewArea::indexAt(const QPoint& pos) const
{
	QPoint containerPos = container->mapFrom(this, pos);

	for (auto it = visibleWidgets.constBegin(); it != visibleWidgets.constEnd(); ++it) {
		if (it.value()->geometry().contains(containerPos)) {
			return it.key();
		}
	}
	return -1;
}

void PreviewArea::updateContainerSize()
{
	if (totalCount == 0) return;

	int rows = (totalCount + currentColumns - 1) / currentColumns;
	int totalHeight = rows * (thumbnailSize + spacing) + spacing;
	widget()->setFixedHeight(totalHeight);
	widget()->setFixedWidth(viewport()->width());
}

void PreviewArea::mousePressEvent(QMouseEvent *event)
{
	int index = indexAt(event->pos());
	if (index >= 0) {
		onThumbnailWidgetClicked(index, event->modifiers());
	}
	else {
		clearSelection();
	}
}

void PreviewArea::mouseDoubleClickEvent(QMouseEvent *event)
{
	int index = indexAt(event->pos());
	if (index >= 0) {
		onThumbnailWidgetDoubleClicked(index);
	}
}

void PreviewArea::setSelection(const QSet<int>& indices)
{
	// Снимаем выделение со старых
	for (int index : selectedIndices) {
		ThumbnailWidget* widget = visibleWidgets.value(index, nullptr);
		if (widget) widget->setSelected(false);
	}

	// Устанавливаем новое выделение
	selectedIndices = indices;
	lastSelectedIndex = selectedIndices.isEmpty() ? -1 : *selectedIndices.begin();

	for (int index : selectedIndices) {
		ThumbnailWidget* widget = visibleWidgets.value(index, nullptr);
		if (widget) widget->setSelected(true);
	}

	emit selectionChanged(selectedIndices);
}

void PreviewArea::clearSelection()
{
	if (selectedIndices.isEmpty()) return;

	for (int index : selectedIndices) {
		ThumbnailWidget* widget = visibleWidgets.value(index, nullptr);
		if (widget) widget->setSelected(false);
	}

	selectedIndices.clear();
	lastSelectedIndex = -1;
	emit selectionChanged(selectedIndices);
}

QStringList PreviewArea::getSelectedFilenames() const
{
	QStringList result;
	for (int index : selectedIndices) {
		if (index >= 0 && index < filenames.size()) {
			result.append(filenames[index]);
		}
	}
	return result;
}

void PreviewArea::resizeEvent(QResizeEvent *event)
{
	QScrollArea::resizeEvent(event);
	updateColumns();
	updateContainerSize();
	updateVisibleRange();
}

void PreviewArea::scrollContentsBy(int dx, int dy)
{
	QScrollArea::scrollContentsBy(dx, dy);
	// Используем таймер для debounce вместо прямого вызова
	scrollTimer->start();
}

void PreviewArea::onThumbnailLoaded(int index, const QPixmap& pixmap)
{
	if (index >= 0 && index < totalCount) {
		setThumbnail(index, pixmap);
	}
}

void PreviewArea::onThumbnailWidgetClicked(int index, Qt::KeyboardModifiers modifiers)
{
	// Логика выделения
	if (modifiers == Qt::NoModifier) {
		clearSelection();
		selectedIndices.insert(index);
		lastSelectedIndex = index;
	}
	else if (modifiers & Qt::ControlModifier) {
		if (selectedIndices.contains(index)) {
			selectedIndices.remove(index);
		}
		else {
			selectedIndices.insert(index);
			lastSelectedIndex = index;
		}
	}
	else if (modifiers & Qt::ShiftModifier && lastSelectedIndex != -1) {
		int start = qMin(lastSelectedIndex, index);
		int end = qMax(lastSelectedIndex, index);
		for (int i = start; i <= end; ++i) {
			selectedIndices.insert(i);
		}
	}

	// Обновляем отображение выделения
	setSelection(selectedIndices);
	emit thumbnailClicked(index, modifiers);
}

void PreviewArea::onThumbnailWidgetDoubleClicked(int index)
{
	emit thumbnailDoubleClicked(index);
}

void PreviewArea::updateBackgroundStyle()
{
	// Меняем фон в зависимости от наличия выделения
	QString style = QString(
		"QWidget {"
		"    background-color: %1;"
		"    border: %2;"
		"}"
	).arg(hasSelection() ? "#f5f5f5" : "#fafafa")
		.arg(hasSelection() ? "1px solid #e0e0e0" : "1px dashed #e0e0e0");

	container->setStyleSheet(style);
}
