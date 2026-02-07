#pragma once

#include <QDockWidget>
#include <QTreeView>
#include <QLabel>
#include <QFileSystemModel>

class QPushButton;

class CategoriesPanel : public QDockWidget
{
	Q_OBJECT

public:
	explicit CategoriesPanel(QWidget *parent);
	~CategoriesPanel();

	// Настройки
	void setTargetRoot(const QString& path);

	// Получение данных
	QString getCurrentCategory() const;	//!
	QString getSelectedCategory() const;//!
	QString getTargetRoot() const { return m_targetRoot; }

	// Обновление UI
	void updateLabels();
	void refreshTree();

signals:
	// Сигналы для внешнего мира
	void moveRequested(const QString& targetCategory);
	void newCategoryRequested(const QString& parentPath);
	void categorySelected(const QString& categoryPath);

public slots:
	void onMoveClicked();
	void onNewCategoryClicked();

private:
	// UI элементы
	QLabel *m_labDst;
	QTreeView *m_categoryTree;
	QFileSystemModel *m_categoriesModel;
	QPushButton *m_moveButton;
	QPushButton *m_newCategoryButton;

	// Данные
	QString m_targetRoot;

private slots:
	void onTreeSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
	void updateTreeRoot();
};
