#pragma once

#include <QObject>
#include <QSet>
#include <QMap>

class TagManager : public QObject
{
	Q_OBJECT

public:
	explicit TagManager(QObject *parent);
	~TagManager();

	// Загрузка/сохранение
	bool loadTags(const QString& tagsFilePath);
	bool saveTags();

	// Работа с тегами объектов
	QSet<QString> getObjectTags(const QString& objectPath);
	bool setObjectTags(const QString& objectPath, const QSet<QString>& tags);

	// Работа с общим списком тегов
	QSet<QString> getAllTags() const { return m_allTags; }
	void addGlobalTag(const QString& tag);
	bool removeGlobalTag(const QString& tag);

	// Вспомогательные
	QString getTagsFilePath() const { return m_tagsFilePath; }
	void setTagsFilePath(const QString& path) { m_tagsFilePath = path; }

signals:
	void tagsChanged(const QString& objectPath);
	void globalTagsChanged();

private:
	QString m_tagsFilePath;
	QSet<QString> m_allTags;
	QMap<QString, QSet<QString>> m_objectTags;  // objectPath -> tags

	bool loadAllTags();
	bool saveAllTags();

	// Для Windows ADS (альтернативные потоки)
	bool loadTagsFromADS(const QString& filePath, QSet<QString>& tags);
	bool saveTagsToADS(const QString& filePath, const QSet<QString>& tags);
};
