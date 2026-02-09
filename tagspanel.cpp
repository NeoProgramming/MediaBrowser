#include "tagspanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QTimer>
#include <algorithm>

TagsPanel::TagsPanel(QWidget *parent)
	: QDockWidget(parent)
	, m_titleLabel(nullptr)
	, m_scrollArea(nullptr)
	, m_tagsContainer(nullptr)
	, m_newTagEdit(nullptr)
	, m_addButton(nullptr)
{
	setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

	// Создаем основной виджет
	QWidget *mainWidget = new QWidget();
	QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

	// Заголовок
	m_titleLabel = new QLabel("Tags: No object selected");
	mainLayout->addWidget(m_titleLabel);

	// Область с тегами и прокруткой
	m_scrollArea = new QScrollArea();
	m_scrollArea->setWidgetResizable(true);
	m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	m_tagsContainer = new QWidget();
	m_scrollArea->setWidget(m_tagsContainer);
	mainLayout->addWidget(m_scrollArea, 1); // Растягиваем

	// Панель добавления нового тега
	QHBoxLayout *addLayout = new QHBoxLayout();
	m_newTagEdit = new QLineEdit();
	m_newTagEdit->setPlaceholderText("Enter new tag...");
	m_addButton = new QPushButton("Add");
	m_addButton->setFixedWidth(60);

	addLayout->addWidget(m_newTagEdit);
	addLayout->addWidget(m_addButton);

	mainLayout->addLayout(addLayout);

	mainWidget->setLayout(mainLayout);
	setWidget(mainWidget);

	// Настройка док-панели
	setFeatures(QDockWidget::DockWidgetMovable |
		QDockWidget::DockWidgetFloatable |
		QDockWidget::DockWidgetClosable);

	// Подключаем сигналы
	connect(m_addButton, &QPushButton::clicked,
		this, &TagsPanel::onAddTagClicked);
	connect(m_newTagEdit, &QLineEdit::returnPressed,
		this, &TagsPanel::onAddTagClicked);
}

TagsPanel::~TagsPanel()
{
}

void TagsPanel::setObjectTags(const QSet<QString>& objectTags)
{
	m_objectTags = objectTags;
	createTagCheckboxes();
	updateTitle();
}

void TagsPanel::setAllTags(const QSet<QString>& allTags)
{
	m_allTags = allTags;
	createTagCheckboxes();
}

void TagsPanel::setObjectName(const QString& name)
{
	m_objectName = name;
	updateTitle();
}

QSet<QString> TagsPanel::getSelectedTags() const
{
	QSet<QString> selected;
	for (auto it = m_tagCheckboxes.constBegin(); it != m_tagCheckboxes.constEnd(); ++it) {
		if (it.value()->isChecked()) {
			selected.insert(it.key());
		}
	}
	return selected;
}

QString TagsPanel::getNewTagText() const
{
	return m_newTagEdit->text().trimmed();
}

void TagsPanel::refreshTags()
{
	createTagCheckboxes();
}

void TagsPanel::clearInput()
{
	m_newTagEdit->clear();
}

void TagsPanel::createTagCheckboxes()
{
	// Удаляем старые чекбоксы
	for (QCheckBox *checkbox : m_tagCheckboxes) {
		disconnect(checkbox, &QCheckBox::toggled,
			this, &TagsPanel::onTagCheckboxToggled);
		delete checkbox;
	}
	m_tagCheckboxes.clear();

	// Создаем два списка: установленные и все остальные теги
	QStringList assignedTags = m_objectTags.values();
	QStringList otherTags = m_allTags.subtract(m_objectTags).values();

	// Сортируем по алфавиту (регистронезависимо)
	std::sort(assignedTags.begin(), assignedTags.end(),
		[](const QString &a, const QString &b) {
		return a.toLower() < b.toLower();
	});

	std::sort(otherTags.begin(), otherTags.end(),
		[](const QString &a, const QString &b) {
		return a.toLower() < b.toLower();
	});

	// Сначала добавляем установленные теги
	for (const QString &tag : assignedTags) {
		QCheckBox *checkbox = new QCheckBox(tag, m_tagsContainer);
		checkbox->setVisible(true);
		checkbox->setChecked(true);

		// Стиль для установленных тегов
		checkbox->setStyleSheet(
			"QCheckBox {"
			"    padding: 4px 8px;"
			"    margin: 2px;"
			"    border-radius: 4px;"
			"    background-color: #ffff9b;"  // Светло-желтый
			"    border: 1px solid #bbdefb;"
			"}"
			"QCheckBox:hover {"
			"    background-color: #ffff00;"  // желтый при наведении
			"}"
			"QCheckBox::indicator {"
			"    width: 16px;"
			"    height: 16px;"
			"}"
		);

		connect(checkbox, &QCheckBox::toggled,
			this, &TagsPanel::onTagCheckboxToggled);
		m_tagCheckboxes[tag] = checkbox;
	}

	// Затем добавляем остальные теги
	for (const QString &tag : otherTags) {
		QCheckBox *checkbox = new QCheckBox(tag, m_tagsContainer);
		checkbox->setVisible(true);
		checkbox->setChecked(false);

		// Стиль для не установленных тегов
		checkbox->setStyleSheet(
			"QCheckBox {"
			"    padding: 4px 8px;"
			"    margin: 2px;"
			"    border-radius: 4px;"
			"    background-color: #f5f5f5;"  // Светло-серый
			"    border: 1px solid #e0e0e0;"
			"}"
			"QCheckBox:hover {"
			"    background-color: #eeeeee;"  // Серый при наведении
			"}"
			"QCheckBox::indicator {"
			"    width: 16px;"
			"    height: 16px;"
			"}"
		);

		connect(checkbox, &QCheckBox::toggled,
			this, &TagsPanel::onTagCheckboxToggled);
		m_tagCheckboxes[tag] = checkbox;
	}

	// Располагаем теги
	rearrangeTags();
}

// Новый метод для ручного размещения тегов
void TagsPanel::rearrangeTags()
{
	if (m_tagCheckboxes.isEmpty() || !m_tagsContainer)
		return;

	// Рассчитываем доступную ширину (с отступами)
	int containerWidth = m_tagsContainer->width() - 20; // 10px отступ с каждой стороны
	int currentX = 10;  // Начинаем слева с отступа
	int currentY = 10;  // Начинаем сверху с отступа
	int rowHeight = 0;
	int hSpacing = 4;   // Горизонтальный отступ между тегами
	int vSpacing = 4;   // Вертикальный отступ между строками

	
	// Располагаем теги слева направо, сверху вниз
	for (auto it = m_tagCheckboxes.constBegin(); it != m_tagCheckboxes.constEnd(); ++it) {
		QCheckBox* checkbox = it.value();

		// Получаем размер тега
		checkbox->adjustSize(); // Обновляем размер после изменения текста
		QSize size = checkbox->sizeHint();

		// Если тег не помещается в текущую строку
		if (currentX + size.width() > containerWidth && currentX > 10) {
			// Переходим на новую строку
			currentX = 10;
			currentY += rowHeight + vSpacing;
			rowHeight = 0;
		}

		// Устанавливаем позицию и размер
		checkbox->setGeometry(QRect(currentX, currentY, size.width(), size.height()));

		// Обновляем позицию для следующего тега
		currentX += size.width() + hSpacing;
		rowHeight = qMax(rowHeight, size.height());
	}

	// Обновляем высоту контейнера
	int totalHeight = currentY + rowHeight + 10;
	m_tagsContainer->setMinimumHeight(totalHeight);
}

// Обработчик изменения размера
void TagsPanel::resizeEvent(QResizeEvent *event)
{
	QDockWidget::resizeEvent(event);

	// Небольшая задержка, чтобы убедиться, что размеры обновились
	QTimer::singleShot(10, this, [this]() {
		if (!m_tagCheckboxes.isEmpty()) {
			rearrangeTags();
		}
	});
}

void TagsPanel::updateTitle()
{
	QString title = "Tags";
	if (!m_objectName.isEmpty()) {
		title += QString(": %1").arg(m_objectName);
	}

	if (!m_objectTags.isEmpty()) {
		title += QString(" (%1 tags)").arg(m_objectTags.size());
	}

	m_titleLabel->setText(title);
}

void TagsPanel::onAddTagClicked()
{
	QString newTag = m_newTagEdit->text().trimmed();

	if (newTag.isEmpty()) {
		return;
	}

	// Проверяем, нет ли уже такого тега
	if (m_allTags.contains(newTag) || m_objectTags.contains(newTag)) {
		m_newTagEdit->clear();
		return;
	}

	emit addTagClicked(newTag);
	m_newTagEdit->clear();
}

void TagsPanel::onTagCheckboxToggled(bool checked)
{
	QCheckBox *checkbox = qobject_cast<QCheckBox*>(sender());
	if (!checkbox) return;

	QString tag = checkbox->text();
	emit tagToggled(tag, checked);

	// Обновляем внутренние данные
	if (checked) {
		m_objectTags.insert(tag);
	}
	else {
		m_objectTags.remove(tag);
	}

	// Отправляем сигнал об изменении всех тегов
	emit tagsChanged(getSelectedTags());

	updateTitle();
}
