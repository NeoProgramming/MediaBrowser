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
	container->setAttribute(Qt::WA_TransparentForMouseEvents);
	setWidget(container);

	// Стиль фона
	container->setStyleSheet(
		"QWidget {"
		"    background-color: #fafafa;"
		"    border: 1px dashed #e0e0e0;"
		"}"
	);

	// Устанавливаем шаг прокрутки для стрелок
	QScrollBar* vScrollBar = verticalScrollBar();
	int rowHeight = thumbnailSize + spacing;
	vScrollBar->setSingleStep(rowHeight);
	
	vScrollBar->update();

	// Устанавливаем политику фокуса для получения событий клавиатуры
//	setFocusPolicy(Qt::StrongFocus);
	updateBackgroundStyle();	

	connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &PreviewArea::updateVisibleRange);

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
}

void PreviewArea::setThumbnailSize(int size)
{
	thumbnailSize = size;
	updateScrollStep();
	updateColumns();
	updateContainerSize();
	//@ updateVisibleRange();
	recreateVisibleWidgets();
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
		updateContainerSize();
		recreateVisibleWidgets();
	}
}

void PreviewArea::setTotalCount(int count)
{
	totalCount = count;
	filenames.resize(count);
	thumbnails.resize(count);
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
	thumbnails.clear();
	filenames.clear();
	selectedIndices.clear();
	totalCount = 0;
	firstVisibleIndex = -1;
	lastVisibleIndex = -1;
}

void PreviewArea::updateVisibleRange()
{
	if (totalCount == 0 || currentColumns == 0)
		return;

	int scrollTop = verticalScrollBar()->value();
	int viewportHeight = viewport()->height();
	int rowHeight = thumbnailSize + spacing;

	// 1. ЧИСТЫЕ границы без буфера
	int firstRow = scrollTop / rowHeight;
	int lastRow = (scrollTop + viewportHeight) / rowHeight;

	// 2. Добавляем небольшой буфер ПОСЛЕ расчёта
	const int bufferRows = 2;
	firstRow = qMax(0, firstRow - bufferRows);

	int maxRow = (totalCount - 1) / currentColumns;
	lastRow = qMin(maxRow, lastRow + bufferRows);

	int newFirst = firstRow * currentColumns;
	int newLast = qMin(totalCount - 1, (lastRow + 1) * currentColumns - 1);

	// 3. Если диапазон тот же — выходим
	if (newFirst == firstVisibleIndex &&
		newLast == lastVisibleIndex)
		return;

	// 4. ПОЛНОЕ пересоздание (пока без оптимизаций)
	qDeleteAll(visibleWidgets);
	visibleWidgets.clear();

	firstVisibleIndex = newFirst;
	lastVisibleIndex = newLast;

	for (int i = newFirst; i <= newLast; ++i)
	{
		ThumbnailWidget* w = createThumbnailWidget(i);

		int row = i / currentColumns;
		int col = i % currentColumns;

		int x = col * rowHeight + spacing / 2;
		int y = row * rowHeight + spacing / 2;

		w->setGeometry(x, y, thumbnailSize, thumbnailSize);
		visibleWidgets.append(w);
	}

	emit visibleRangeChanged(newFirst, newLast);
}

// Вспомогательный метод создания виджета
ThumbnailWidget* PreviewArea::createThumbnailWidget(int index)
{
	ThumbnailWidget* widget = new ThumbnailWidget(index, container);
	widget->setFixedSize(thumbnailSize, thumbnailSize);

	if (!thumbnails[index].isNull()) {
		widget->setPixmap(thumbnails[index]);
		widget->setText("");
	}
	else {
		widget->setText(filenames[index]);
	}

	widget->setSelected(selectedIndices.contains(index));

	connect(widget, &ThumbnailWidget::clicked,
		this, &PreviewArea::onThumbnailWidgetClicked);
	connect(widget, &ThumbnailWidget::doubleClicked,
		this, &PreviewArea::onThumbnailWidgetDoubleClicked);

	widget->show();
	return widget;
}

ThumbnailWidget* PreviewArea::getOrCreateWidget(int index)
{
	ThumbnailWidget* widget = visibleWidgets.value(index, nullptr);
	if (!widget) {
		widget = new ThumbnailWidget(index, container);
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
	thumbnails[index] = pixmap;

	// Обновляем виджет, если он видим
	int offset = index - firstVisibleIndex;
	if (offset >= 0 && offset < visibleWidgets.size()) {
		ThumbnailWidget* widget = visibleWidgets[offset];
		if (widget) {
			widget->setPixmap(pixmap);
			widget->setText("");
		}
	}
}

void PreviewArea::setFilename(int index, const QString& filename)
{
	if (index < 0 || index >= totalCount) return;
	filenames[index] = filename;
	
	// Обновляем текст в виджете, если он видим
	int offset = index - firstVisibleIndex;
	if (offset >= 0 && offset < visibleWidgets.size()) {
		ThumbnailWidget* widget = visibleWidgets[offset];
		if (widget && thumbnails[index].isNull()) {
			widget->setText(filename);
		}
	}
}

int PreviewArea::indexAt(const QPoint& pos) const
{
	QPoint containerPos = container->mapFrom(this, pos);

	for (int i = 0; i < visibleWidgets.size(); ++i) {
		if (visibleWidgets[i] && visibleWidgets[i]->geometry().contains(containerPos)) {
			return firstVisibleIndex + i;
		}
	}
	return -1;
}

void PreviewArea::updateContainerSize()
{
	if (totalCount == 0 || currentColumns == 0) {
		widget()->setFixedHeight(0);
		return;
	}

	int rows = (totalCount + currentColumns - 1) / currentColumns;
	int totalHeight = rows * (thumbnailSize + spacing) + spacing;
	widget()->setFixedHeight(totalHeight);
	widget()->setFixedWidth(viewport()->width());
}

void PreviewArea::recreateVisibleWidgets()
{
	// Удаляем старые виджеты
	for (auto widget : visibleWidgets) {
		if (widget) delete widget;
	}
	visibleWidgets.clear();

	if (totalCount == 0 || firstVisibleIndex < 0) return;

	// Определяем, сколько виджетов нужно создать
	int viewportHeight = viewport()->height();
	int rowsVisible = (viewportHeight + thumbnailSize + spacing - 1) / (thumbnailSize + spacing) + 2; // +2 для буфера
	int widgetsNeeded = rowsVisible * currentColumns;

	// Создаем новые виджеты
	for (int i = 0; i < widgetsNeeded; ++i) {
		int fileIndex = firstVisibleIndex + i;
		if (fileIndex >= totalCount) break;

		ThumbnailWidget* widget = new ThumbnailWidget(fileIndex, container);
		widget->setFixedSize(thumbnailSize, thumbnailSize);

		// Устанавливаем содержимое
		if (!thumbnails[fileIndex].isNull()) {
			widget->setPixmap(thumbnails[fileIndex]);
			widget->setText("");
		}
		else {
			widget->setText(filenames[fileIndex]);
		}

		widget->setSelected(selectedIndices.contains(fileIndex));

		// Позиционируем
		int row = i / currentColumns;
		int col = i % currentColumns;
		int x = col * (thumbnailSize + spacing) + spacing / 2;
		int y = row * (thumbnailSize + spacing) + spacing / 2;
		widget->setGeometry(x, y, thumbnailSize, thumbnailSize);

		// Подключаем сигналы
		connect(widget, &ThumbnailWidget::clicked,
			this, &PreviewArea::onThumbnailWidgetClicked);
		connect(widget, &ThumbnailWidget::doubleClicked,
			this, &PreviewArea::onThumbnailWidgetDoubleClicked);

		widget->show();
		visibleWidgets.append(widget);
	}
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
		int offset = index - firstVisibleIndex;
		if (offset >= 0 && offset < visibleWidgets.size()) {
			if (visibleWidgets[offset]) {
				visibleWidgets[offset]->setSelected(false);
			}
		}
	}

	// Устанавливаем новое выделение
	selectedIndices = indices;
	lastSelectedIndex = selectedIndices.isEmpty() ? -1 : *selectedIndices.begin();

	for (int index : selectedIndices) {
		int offset = index - firstVisibleIndex;
		if (offset >= 0 && offset < visibleWidgets.size()) {
			if (visibleWidgets[offset]) {
				visibleWidgets[offset]->setSelected(true);
			}
		}
	}

	emit selectionChanged(selectedIndices);
}

void PreviewArea::clearSelection()
{
	if (selectedIndices.isEmpty()) return;

	for (int index : selectedIndices) {
		int offset = index - firstVisibleIndex;
		if (offset >= 0 && offset < visibleWidgets.size()) {
			if (visibleWidgets[offset]) {
				visibleWidgets[offset]->setSelected(false);
			}
		}
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
	updateScrollBarRange();
	updateScrollStep();
	updateColumns();
	updateContainerSize();
	//updateVisibleRange();
	recreateVisibleWidgets();
}

void PreviewArea::scrollContentsBy(int dx, int dy)
{
	QScrollArea::scrollContentsBy(dx, dy);
	
	// Таймер оставляем только для отложенной загрузки превью
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

void PreviewArea::removeFiles(const QList<int>& removedIndices)
{
	if (removedIndices.isEmpty()) return;

	qDebug() << "PreviewArea::removeFiles" << removedIndices;

	// Сортируем индексы для обработки
	QList<int> sortedIndices = removedIndices;
	std::sort(sortedIndices.begin(), sortedIndices.end());

	// 1. Удаляем из массивов (в обратном порядке)
	for (int i = sortedIndices.size() - 1; i >= 0; --i) {
		int index = sortedIndices[i];
		filenames.removeAt(index);
		thumbnails.removeAt(index);
	}

	// 2. Обновляем общее количество
	int oldTotal = totalCount;
	totalCount = filenames.size();
	int removedCount = oldTotal - totalCount;

	// 3. Обновляем выделение
	QSet<int> newSelected;
	for (int selIndex : selectedIndices) {
		int newIndex = selIndex;
		for (int removedIndex : sortedIndices) {
			if (selIndex > removedIndex) {
				newIndex--;
			}
		}
		if (newIndex >= 0 && newIndex < totalCount) {
			newSelected.insert(newIndex);
		}
	}
	selectedIndices = newSelected;
	lastSelectedIndex = selectedIndices.isEmpty() ? -1 : *selectedIndices.begin();

	// 4. Сдвигаем индексы в видимых виджетах
	shiftIndicesAfterRemoval(removedCount);

	// 5. Обновляем размер контейнера
	updateContainerSize();

	// 6. Перестраиваем видимые виджеты
	recreateVisibleWidgets();

	emit selectionChanged(selectedIndices);
}

void PreviewArea::removeFile(int index)
{
	removeFiles({ index });
}

void PreviewArea::shiftIndicesAfterRemoval(int removedCount)
{
	// Просто сдвигаем firstVisibleIndex
	firstVisibleIndex -= removedCount;
	if (firstVisibleIndex < 0) firstVisibleIndex = 0;

	// Обновляем индексы в существующих виджетах
	for (int i = 0; i < visibleWidgets.size(); ++i) {
		if (visibleWidgets[i]) {
			int newIndex = firstVisibleIndex + i;
			visibleWidgets[i]->setIndex(newIndex);

			// Обновляем содержимое виджета
			if (newIndex < totalCount) {
				if (!thumbnails[newIndex].isNull()) {
					visibleWidgets[i]->setPixmap(thumbnails[newIndex]);
					visibleWidgets[i]->setText("");
				}
				else {
					visibleWidgets[i]->setText(filenames[newIndex]);
				}
				visibleWidgets[i]->setSelected(selectedIndices.contains(newIndex));
			}
		}
	}
}

void PreviewArea::wheelEvent(QWheelEvent *event)
{
	int step = thumbnailSize + spacing;
	int currentValue = verticalScrollBar()->value();
	int numSteps = event->angleDelta().y() / 120; // стандартный шаг колесика

	// Прокручиваем на целое количество рядов
	int newValue = currentValue - numSteps * step;
	newValue = qBound(0, newValue, verticalScrollBar()->maximum());

	verticalScrollBar()->setValue(newValue);
	event->accept();
}

void PreviewArea::updateScrollStep()
{
	QScrollBar* vScrollBar = verticalScrollBar();
	if (!vScrollBar) return;

	int rowHeight = thumbnailSize + spacing;
	
	vScrollBar->setSingleStep(rowHeight);
}

void PreviewArea::updateScrollBarRange()
{
	QScrollBar* bar = verticalScrollBar();

	int totalHeight = widget()->height();
	int viewportHeight = viewport()->height();

	bar->setMinimum(0);
	bar->setMaximum(qMax(0, totalHeight - viewportHeight));

	// PageStep = ровно высота видимой области
	bar->setPageStep(viewportHeight);

	// SingleStep = ровно одна строка
	bar->setSingleStep(thumbnailSize + spacing);
}