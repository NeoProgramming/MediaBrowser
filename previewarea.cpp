#include "previewarea.h"
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>
#include <QGridLayout>

PreviewArea::PreviewArea(QWidget *parent)
	: QScrollArea(parent)
	, thumbnailSize(200)
	, columns(4)
	, lastSelectedIndex(-1)
	, container(nullptr)
	, layout(nullptr)
{
	// Настройка области прокрутки
	setWidgetResizable(true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	// Создаем контейнер
	container = new QWidget();
	layout = new QGridLayout(container);
	layout->setSpacing(10);
	layout->setContentsMargins(15, 15, 15, 15);

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
	updateBackground();
}

PreviewArea::~PreviewArea()
{
	// Удаляем eventFilter при разрушении
	if (container) {
		container->removeEventFilter(this);
	}
}

bool PreviewArea::eventFilter(QObject *obj, QEvent *event)
{
	// Обработка клика по фону контейнера
	if (obj == container && event->type() == QEvent::MouseButtonPress)
	{
		QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

		if (mouseEvent->button() == Qt::LeftButton)
		{
			// Проверяем, не кликнули ли по превьюшке
			QWidget *child = container->childAt(mouseEvent->pos());

			// Если кликнули не на превьюшке - снимаем выделение
			if (!child) {
				clearSelection();
				return true; // Событие обработано
			}

			// Проверяем, не является ли child ThumbnailWidget
			ThumbnailWidget *thumbnail = qobject_cast<ThumbnailWidget*>(child);
			if (!thumbnail || !thumbnailWidgets.contains(thumbnail)) {
				clearSelection();
				return true;
			}
		}
	}

	return QScrollArea::eventFilter(obj, event);
}

void PreviewArea::setThumbnailSize(int size)
{
	thumbnailSize = size;
	// Пересоздаем все превью с новым размером
	for (ThumbnailWidget *widget : thumbnailWidgets) {
		if (widget) {
			widget->setFixedSize(thumbnailSize + 20, thumbnailSize + 20);
		}
	}
}

void PreviewArea::setColumns(int cols)
{
	columns = cols;
	// Перерисовываем layout
	for (int i = 0; i < thumbnailWidgets.size(); ++i) {
		int row = i / columns;
		int col = i % columns;
		layout->addWidget(thumbnailWidgets[i], row, col, Qt::AlignCenter);
	}
	container->adjustSize();
}

void PreviewArea::clearThumbnails()
{
	clearSelection();

	for (ThumbnailWidget *widget : thumbnailWidgets) {
		if (widget) {
			layout->removeWidget(widget);
			delete widget;
		}
	}
	thumbnailWidgets.clear();
	filenames.clear();

	container->adjustSize();
}

void PreviewArea::addThumbnail(int index, const QString& filename, const QPixmap& pixmap)
{
	// Если нужно создать новый виджет
	while (index >= thumbnailWidgets.size()) {
		ThumbnailWidget *widget = new ThumbnailWidget(thumbnailWidgets.size(), container);
		widget->setFixedSize(thumbnailSize + 20, thumbnailSize + 20);
		widget->setText("Loading...");

		// Подключаем сигналы
		connect(widget, &ThumbnailWidget::clicked,
			this, &PreviewArea::onThumbnailWidgetClicked);
		connect(widget, &ThumbnailWidget::doubleClicked,
			this, &PreviewArea::onThumbnailWidgetDoubleClicked);

		int row = thumbnailWidgets.size() / columns;
		int col = thumbnailWidgets.size() % columns;
		layout->addWidget(widget, row, col, Qt::AlignCenter);

		thumbnailWidgets.append(widget);
		filenames.append("");
	}

	// Обновляем виджет
	if (index < thumbnailWidgets.size()) {
		ThumbnailWidget *widget = thumbnailWidgets[index];
		QPixmap scaled = pixmap.scaled(thumbnailSize, thumbnailSize,
			Qt::KeepAspectRatio, Qt::SmoothTransformation);
		widget->setPixmap(scaled);
		widget->setText("");
		widget->setToolTip(filename);
		filenames[index] = filename;
	}

	container->adjustSize();
}

void PreviewArea::setPlaceholder(int index, const QString& text)
{
	if (index >= 0 && index < thumbnailWidgets.size()) {
		ThumbnailWidget *widget = thumbnailWidgets[index];
		widget->setText(text);
		widget->setPixmap(QPixmap());
	}
}

void PreviewArea::onThumbnailLoaded(int index, const QPixmap& pixmap)
{
	if (index >= 0 && index < thumbnailWidgets.size()) {
		addThumbnail(index, filenames[index], pixmap);
	}
}

void PreviewArea::clearSelection()
{
	if (selectedIndices.isEmpty()) return;

	for (int index : selectedIndices) {
		if (index >= 0 && index < thumbnailWidgets.size()) {
			thumbnailWidgets[index]->setSelected(false);
		}
	}

	selectedIndices.clear();
	lastSelectedIndex = -1;
	updateBackground();
	emit selectionCleared();
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

void PreviewArea::onThumbnailWidgetClicked(int index, Qt::KeyboardModifiers modifiers)
{
	// Логика выделения (можно вынести в отдельный метод)
	if (modifiers == Qt::NoModifier) {
		clearSelection();
		selectedIndices.insert(index);
		thumbnailWidgets[index]->setSelected(true);
		lastSelectedIndex = index;
	}
	else if (modifiers & Qt::ControlModifier) {
		if (selectedIndices.contains(index)) {
			selectedIndices.remove(index);
			thumbnailWidgets[index]->setSelected(false);
		}
		else {
			selectedIndices.insert(index);
			thumbnailWidgets[index]->setSelected(true);
		}
		lastSelectedIndex = index;
	}
	else if (modifiers & Qt::ShiftModifier && lastSelectedIndex != -1) {
		int start = qMin(lastSelectedIndex, index);
		int end = qMax(lastSelectedIndex, index);
		for (int i = start; i <= end; ++i) {
			if (!selectedIndices.contains(i)) {
				selectedIndices.insert(i);
				thumbnailWidgets[i]->setSelected(true);
			}
		}
	}

	updateBackground();
	emit thumbnailClicked(index, modifiers);
	emit selectionChanged(selectedIndices);
}

void PreviewArea::onThumbnailWidgetDoubleClicked(int index)
{
	emit thumbnailDoubleClicked(index);
}

void PreviewArea::onContainerClicked()
{
	clearSelection();
}

void PreviewArea::updateBackground()
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

void PreviewArea::updateSelection(int index, Qt::KeyboardModifiers modifiers)
{
	if (index < 0 || index >= thumbnailWidgets.size()) return;

	// Без модификаторов - одиночное выделение
	if (modifiers == Qt::NoModifier) {
		clearSelection();
		selectedIndices.insert(index);
		thumbnailWidgets[index]->setSelected(true);
		lastSelectedIndex = index;
	}
	// Ctrl - добавить/удалить из выделения
	else if (modifiers & Qt::ControlModifier) {
		if (selectedIndices.contains(index)) {
			selectedIndices.remove(index);
			thumbnailWidgets[index]->setSelected(false);
			if (lastSelectedIndex == index) {
				lastSelectedIndex = selectedIndices.isEmpty() ? -1 : *selectedIndices.begin();
			}
		}
		else {
			selectedIndices.insert(index);
			thumbnailWidgets[index]->setSelected(true);
			lastSelectedIndex = index;
		}
	}
	// Shift - выделить диапазон от lastSelectedIndex до index
	else if (modifiers & Qt::ShiftModifier && lastSelectedIndex != -1) {
		int start = qMin(lastSelectedIndex, index);
		int end = qMax(lastSelectedIndex, index);

		for (int i = start; i <= end; ++i) {
			if (!selectedIndices.contains(i)) {
				selectedIndices.insert(i);
				thumbnailWidgets[i]->setSelected(true);
			}
		}
	}
	// Alt - исключить из выделения
	else if (modifiers & Qt::AltModifier) {
		if (selectedIndices.contains(index)) {
			selectedIndices.remove(index);
			thumbnailWidgets[index]->setSelected(false);
			if (lastSelectedIndex == index) {
				lastSelectedIndex = selectedIndices.isEmpty() ? -1 : *selectedIndices.begin();
			}
		}
	}

	updateBackground();
}
