#pragma once

#include <QDockWidget>
#include <QSet>
#include <QMap>

class QLabel;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QScrollArea;

class TagsPanel : public QDockWidget
{
	Q_OBJECT

public:
	explicit TagsPanel(QWidget *parent);
	~TagsPanel();

	// Установка данных
	void setObjectTags(const QSet<QString>& objectTags);
	void setAllTags(const QSet<QString>& allTags);
	void setObjectName(const QString& name);

	void updateObjectTagsData(const QSet<QString>& objectTags);
	void updateAllTagsData(const QSet<QString>& allTags);

	// Получение данных
	QSet<QString> getSelectedTags() const;
	QString getNewTagText() const;

	// Обновление UI

	void clearInput();

signals:
	void tagToggled(const QString& tag, bool checked);
	void addTagClicked(const QString& newTag);
	void tagsChanged(const QSet<QString>& newTags);

public slots:
	void onAddTagClicked();
protected:
	void resizeEvent(QResizeEvent *event) override; 
private:
	// UI элементы
	QLabel *m_titleLabel;
	QScrollArea *m_scrollArea;
	QWidget *m_tagsContainer;
	QLineEdit *m_newTagEdit;
	QPushButton *m_addButton;

	// Данные
	QSet<QString> m_objectTags;    // Теги текущего объекта
	QSet<QString> m_allTags;       // Все теги системы
	QString m_objectName;

	// Виджеты тегов
	QMap<QString, QCheckBox*> m_tagCheckboxes;

	// Флаги состояния
	bool m_needsRefresh;

	// Вспомогательные методы
	void refreshTags();
	void createTagCheckboxes();
	void updateTitle();
	void rearrangeTags(); 
	void createTagCheckbox(const QString& tag, bool checked);
private slots:
	void onTagCheckboxToggled(bool checked);
};
