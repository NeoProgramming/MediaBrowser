#include "categoriespanel.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDir>
#include <QDebug>
#include <QMessageBox>

CategoriesPanel::CategoriesPanel(QWidget *parent)
	: QDockWidget(parent)
	, m_labDst(nullptr)
	, m_categoryTree(nullptr)
	, m_categoriesModel(nullptr)
	, m_moveButton(nullptr)
	, m_newCategoryButton(nullptr)
{
	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	// Создаем виджет для содержимого док-панели
	QWidget *contentWidget = new QWidget();
	QVBoxLayout *mainLayout = new QVBoxLayout(contentWidget);

	// Создаем UI элементы
	m_labDst = new QLabel("Dst: <NOT SELECTED>");
	m_moveButton = new QPushButton("MOVE");
	m_categoryTree = new QTreeView();
	m_newCategoryButton = new QPushButton("New theme");

	// Настраиваем дерево
	m_categoryTree->setHeaderHidden(true);
	m_categoryTree->setAnimated(true);

	// Создаем модель файловой системы
	m_categoriesModel = new QFileSystemModel(this);
	m_categoriesModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
	m_categoryTree->setModel(m_categoriesModel);

	// Скрываем ненужные колонки
	for (int i = 1; i < m_categoriesModel->columnCount(); ++i) {
		m_categoryTree->hideColumn(i);
	}

	// Стиль выделения
	QString styleSheet =
		"QTreeView::item:selected:!active {"
		"    background-color: #2196F3;"
		"    color: #000000;"
		"}";
	m_categoryTree->setStyleSheet(styleSheet);

	// Размещаем элементы
	mainLayout->addWidget(m_moveButton);
	mainLayout->addWidget(m_labDst);
	mainLayout->addWidget(m_categoryTree, 1); // Растягиваем дерево
	mainLayout->addWidget(m_newCategoryButton);
	mainLayout->addStretch();

	contentWidget->setLayout(mainLayout);
	setWidget(contentWidget);

	// Настройка док-панели
	setFeatures(QDockWidget::DockWidgetMovable |
		QDockWidget::DockWidgetFloatable |
		QDockWidget::DockWidgetClosable);

	// Подключаем сигналы
	connect(m_moveButton, &QPushButton::clicked,
		this, &CategoriesPanel::onMoveClicked);
	connect(m_newCategoryButton, &QPushButton::clicked,
		this, &CategoriesPanel::onNewCategoryClicked);
	connect(m_categoryTree->selectionModel(), &QItemSelectionModel::selectionChanged,
		this, &CategoriesPanel::onTreeSelectionChanged);
}

CategoriesPanel::~CategoriesPanel()
{
}

void CategoriesPanel::setTargetRoot(const QString& path)
{
	m_targetRoot = path;

	if (!m_targetRoot.isEmpty() && QDir(m_targetRoot).exists()) {
		m_categoriesModel->setRootPath(m_targetRoot);
		m_categoryTree->setRootIndex(m_categoriesModel->index(m_targetRoot));
	}

	updateLabels();
}

QString CategoriesPanel::getCurrentCategory() const
{
	QModelIndexList selected = m_categoryTree->selectionModel()->selectedIndexes();
	if (!selected.isEmpty()) {
		return m_categoriesModel->filePath(selected.first());
	}
	return m_targetRoot; // Если ничего не выбрано, используем корень
}

QString CategoriesPanel::getSelectedCategory() const
{
	QModelIndexList selected = m_categoryTree->selectionModel()->selectedIndexes();
	if (!selected.isEmpty()) {
		return m_categoriesModel->filePath(selected.first());
	}
	return QString();
}

void CategoriesPanel::updateLabels()
{
	// Обновляем label цели
	QString dstText = "Dst: ";
	if (m_targetRoot.isEmpty()) {
		dstText += "<NOT SELECTED>";
	}
	else {
		dstText += m_targetRoot;

		// Добавляем выбранную категорию
		QString selected = getSelectedCategory();
		if (!selected.isEmpty() && selected != m_targetRoot) {
			QString relPath = QDir(m_targetRoot).relativeFilePath(selected);
			dstText += QString(" / %1").arg(relPath);
		}
	}
	m_labDst->setText(dstText);
}

void CategoriesPanel::refreshTree()
{
	if (!m_targetRoot.isEmpty()) {
		m_categoriesModel->setRootPath("");
		m_categoriesModel->setRootPath(m_targetRoot);
		m_categoryTree->setRootIndex(m_categoriesModel->index(m_targetRoot));
	}
}

void CategoriesPanel::onMoveClicked()
{
	QString targetCategory = getCurrentCategory();
	if (targetCategory.isEmpty()) {
		QMessageBox::warning(this, "Error", "No target category selected");
		return;
	}

	emit moveRequested(targetCategory);
}

void CategoriesPanel::onNewCategoryClicked()
{
	QString parentPath = getSelectedCategory();
	if (parentPath.isEmpty()) {
		parentPath = m_targetRoot;
	}

	if (parentPath.isEmpty()) {
		QMessageBox::warning(this, "Error", "Please select target root first");
		return;
	}

	emit newCategoryRequested(parentPath);
}

void CategoriesPanel::onTreeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
	Q_UNUSED(deselected);

	if (!selected.isEmpty()) {
		QModelIndex index = selected.indexes().first();
		QString path = m_categoriesModel->filePath(index);
		emit categorySelected(path);
	}

	updateLabels();
}

void CategoriesPanel::updateTreeRoot()
{
	if (!m_targetRoot.isEmpty() && QDir(m_targetRoot).exists()) {
		m_categoriesModel->setRootPath(m_targetRoot);
		m_categoryTree->setRootIndex(m_categoriesModel->index(m_targetRoot));
	}
}
