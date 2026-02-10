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
	, m_needsRefresh(false)
{
	setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

	// Создаем основной виджет
	QWidget *mainWidget = new QWidget();
	QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

	// Заголовок
	m_titleLabel = new QLabel("No object selected");
	m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // Растягивается по горизонтали
	m_titleLabel->setWordWrap(false); // Запрещаем перенос слов
	m_titleLabel->setMinimumHeight(30); // Фиксированная высота
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
//	m_objectTags = objectTags;
//	createTagCheckboxes();
//	updateTitle();
	updateObjectTagsData(objectTags);
}

void TagsPanel::setAllTags(const QSet<QString>& allTags)
{
//	m_allTags = allTags;
//	createTagCheckboxes();
	updateAllTagsData(allTags);
}

void TagsPanel::updateObjectTagsData(const QSet<QString>& objectTags)
{
	m_objectTags = objectTags;
	m_needsRefresh = true;

	// Откладываем обновление на следующий цикл событий
	QTimer::singleShot(0, this, &TagsPanel::refreshTags);
}

void TagsPanel::updateAllTagsData(const QSet<QString>& allTags)
{
	m_allTags = allTags;
	m_needsRefresh = true;

	// Откладываем обновление на следующий цикл событий
	QTimer::singleShot(0, this, &TagsPanel::refreshTags);
}

void TagsPanel::refreshTags()
{
	if (!m_needsRefresh) {
		return;
	}

	m_needsRefresh = false;

	qDebug() << "TagsPanel::refreshTags - object tags:" << m_objectTags.size()
		<< "all tags:" << m_allTags.size();

	// Очищаем старые чекбоксы
	for (QCheckBox *checkbox : m_tagCheckboxes) {
		if (checkbox) {
			checkbox->blockSignals(true);
			delete checkbox;
		}
	}
	m_tagCheckboxes.clear();

	if (!m_tagsContainer) {
		return;
	}

	// Объединяем все теги: теги объекта + все теги системы
	QSet<QString> allTagsToShow = m_allTags.unite(m_objectTags);

	if (allTagsToShow.isEmpty()) {
		m_tagsContainer->setMinimumHeight(0);
		return;
	}

	// Разделяем на отмеченные (из объекта) и неотмеченные
	QSet<QString> checkedTags = m_objectTags;
	QSet<QString> uncheckedTags = allTagsToShow.subtract(m_objectTags);

	// Преобразуем в списки и сортируем
	QStringList checkedTagsSorted = checkedTags.values();
	QStringList uncheckedTagsSorted = uncheckedTags.values();

	std::sort(checkedTagsSorted.begin(), checkedTagsSorted.end(),
		[](const QString &a, const QString &b) {
		return a.toLower() < b.toLower();
	});

	std::sort(uncheckedTagsSorted.begin(), uncheckedTagsSorted.end(),
		[](const QString &a, const QString &b) {
		return a.toLower() < b.toLower();
	});

	// Создаем чекбоксы в правильном порядке
	for (const QString &tag : checkedTagsSorted) {
		createTagCheckbox(tag, true);
	}

	for (const QString &tag : uncheckedTagsSorted) {
		createTagCheckbox(tag, false);
	}

	// Перераспределяем теги
	rearrangeTags();
	updateTitle();
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

void TagsPanel::clearInput()
{
	m_newTagEdit->clear();
}

void TagsPanel::createTagCheckbox(const QString& tag, bool checked)
{
	if (m_tagCheckboxes.contains(tag)) {
		return; // Уже существует
	}

	QCheckBox *checkbox = new QCheckBox(tag, m_tagsContainer);
	checkbox->setChecked(checked);
	checkbox->setVisible(true);
	// Настраиваем стиль в зависимости от состояния
	QString backgroundColor = checked ? "#e3f2fd" : "#f5f5f5";
	QString borderColor = checked ? "#bbdefb" : "#e0e0e0";
	QString hoverColor = checked ? "#bbdefb" : "#eeeeee";

	checkbox->setStyleSheet(QString(
		"QCheckBox {"
		"    padding: 4px 8px;"
		"    margin: 2px;"
		"    border-radius: 4px;"
		"    background-color: %1;"
		"    border: 1px solid %2;"
		"}"
		"QCheckBox:hover {"
		"    background-color: %3;"
		"}"
		"QCheckBox::indicator {"
		"    width: 16px;"
		"    height: 16px;"
		"}"
	).arg(backgroundColor, borderColor, hoverColor));

	connect(checkbox, &QCheckBox::toggled,
		this, &TagsPanel::onTagCheckboxToggled);

	m_tagCheckboxes[tag] = checkbox;
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
	int containerWidth = m_tagsContainer->width();
	if (containerWidth <= 0)
		return;

	int availableWidth = containerWidth - 20; // 10px отступ с каждой стороны
	int currentX = 10;  // Начинаем слева с отступа
	int currentY = 10;  // Начинаем сверху с отступа
	int rowHeight = 0;
	int hSpacing = 4;   // Горизонтальный отступ между тегами
	int vSpacing = 4;   // Вертикальный отступ между строками

	// Разделяем теги на отмеченные и неотмеченные
	QMap<QString, QCheckBox*> checkedBoxes;
	QMap<QString, QCheckBox*> uncheckedBoxes;

	for (auto it = m_tagCheckboxes.constBegin(); it != m_tagCheckboxes.constEnd(); ++it) {
		if (!it.value()) continue;

		if (it.value()->isChecked()) {
			checkedBoxes[it.key()] = it.value();
		}
		else {
			uncheckedBoxes[it.key()] = it.value();
		}
	}

	
	// Располагаем теги слева направо, сверху вниз
	/*
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
	}*/

	// Функция для размещения группы тегов
	auto layoutGroup = [&](QMap<QString, QCheckBox*>& group) {
		for (auto it = group.constBegin(); it != group.constEnd(); ++it) {
			QCheckBox* checkbox = it.value();
			if (!checkbox) continue;

			checkbox->adjustSize();
			QSize size = checkbox->sizeHint();

			// Обработка очень длинных тегов
			if (size.width() > availableWidth - 20) {
				size.setWidth(availableWidth - 20);
				checkbox->setFixedWidth(size.width());
			}

			// Если тег не помещается в текущую строку
			if (currentX + size.width() > availableWidth && currentX > 10) {
				currentX = 10;
				currentY += rowHeight + vSpacing;
				rowHeight = 0;
			}

			checkbox->setGeometry(QRect(currentX, currentY, size.width(), size.height()));
			currentX += size.width() + hSpacing;
			rowHeight = qMax(rowHeight, size.height());
		}
	};

	// Сначала размещаем отмеченные теги
	layoutGroup(checkedBoxes);

	// Добавляем отступ между группами
	if (!checkedBoxes.isEmpty() && !uncheckedBoxes.isEmpty()) {
		currentX = 10;
		currentY += rowHeight + vSpacing * 2;
		rowHeight = 0;
	}

	// Затем размещаем неотмеченные теги
	layoutGroup(uncheckedBoxes);

	// Обновляем высоту контейнера
	int totalHeight = currentY + rowHeight + 10;
	m_tagsContainer->setMinimumHeight(totalHeight);
}

// Обработчик изменения размера
void TagsPanel::resizeEvent(QResizeEvent *event)
{
	QDockWidget::resizeEvent(event);
	
	QTimer::singleShot(0, this, &TagsPanel::updateTitle);
	
	QTimer::singleShot(0, this, &TagsPanel::rearrangeTags);
}

void TagsPanel::updateTitle()
{
	QString title;
	QString styleSheet = ""; // По умолчанию

//	if (!m_objectTags.isEmpty()) {
//		title += QString("%1: ").arg(m_objectTags.size());
//	}

	if (!m_objectName.isEmpty()) {
		// Определяем, это файл или папка (по наличию расширения)
		bool isFile = m_objectName.contains('.') &&
			!m_objectName.endsWith('.') &&
			!m_objectName.startsWith('.');

		if (isFile) {
			// Стиль для файла
			styleSheet = "QLabel {"
				"    color: #1976D2;"      // Темно-синий
				"    font-weight: bold;"
				"    background-color: #E3F2FD;"  // Светло-голубой фон
				"    padding: 4px 8px;"
				"    border-radius: 4px;"
				"    border: 1px solid #90CAF9;"
				"}";
			title += QString("%1").arg(m_objectName);
		}
		else {
			// Стиль для папки
			styleSheet = "QLabel {"
				"    color: #388E3C;"      // Темно-зеленый
				"    font-weight: bold;"
				"    background-color: #E8F5E9;"  // Светло-зеленый фон
				"    padding: 4px 8px;"
				"    border-radius: 4px;"
				"    border: 1px solid #A5D6A7;"
				"}";
			title += QString("%1").arg(m_objectName);
		}
	}
	else {
		// Без объекта
		styleSheet = "QLabel {"
			"    color: #757575;"      // Серый
			"    font-style: italic;"
			"}";
		title += ": No object selected";
	}	

	// Устанавливаем текст и стиль
	m_titleLabel->setText(title);
	m_titleLabel->setStyleSheet(styleSheet);

	// Обрезаем длинный текст с многоточием
	QFontMetrics metrics(m_titleLabel->font());
	int labelWidth = m_titleLabel->width() - 16; // Учитываем padding
	QString elidedText = metrics.elidedText(title, Qt::ElideRight, labelWidth);
	m_titleLabel->setText(elidedText);
	m_titleLabel->setToolTip(title); // Полный текст в tooltip

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
//	emit tagToggled(tag, checked);

	// Обновляем внутренние данные
	if (checked) {
		m_objectTags.insert(tag);
	}
	else {
		m_objectTags.remove(tag);
	}

	// Обновляем стиль
	QString backgroundColor = checked ? "#e3f2fd" : "#f5f5f5";
	QString borderColor = checked ? "#bbdefb" : "#e0e0e0";
	QString hoverColor = checked ? "#bbdefb" : "#eeeeee";

	checkbox->setStyleSheet(QString(
		"QCheckBox {"
		"    padding: 4px 8px;"
		"    margin: 2px;"
		"    border-radius: 4px;"
		"    background-color: %1;"
		"    border: 1px solid %2;"
		"}"
		"QCheckBox:hover {"
		"    background-color: %3;"
		"}"
		"QCheckBox::indicator {"
		"    width: 16px;"
		"    height: 16px;"
		"}"
	).arg(backgroundColor, borderColor, hoverColor));

	// Отправляем сигналы
	emit tagToggled(tag, checked);
	emit tagsChanged(getSelectedTags());

	updateTitle();

	// Теперь просто меняем порядок отображения без пересоздания
	QTimer::singleShot(0, this, &TagsPanel::rearrangeTags);
}
