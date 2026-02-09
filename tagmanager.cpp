#include "tagmanager.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>

TagManager::TagManager(QObject *parent)
	: QObject(parent)
{
}

TagManager::~TagManager()
{
}

bool TagManager::loadTags(const QString& tagsFilePath)
{
	m_tagsFilePath = tagsFilePath;
	return loadAllTags();
}

bool TagManager::saveTags()
{
	return saveAllTags();
}

bool TagManager::loadAllTags()
{
	if (m_tagsFilePath.isEmpty()) {
		return false;
	}

	QFile file(m_tagsFilePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qDebug() << "Cannot open tags file:" << m_tagsFilePath << file.errorString();
		return false;
	}

	QTextStream in(&file);
	m_allTags.clear();

	while (!in.atEnd()) {
		QString line = in.readLine().trimmed();
		if (!line.isEmpty()) {
			m_allTags.insert(line);
		}
	}

	file.close();
	qDebug() << "Loaded" << m_allTags.size() << "tags from" << m_tagsFilePath;
	return true;
}

bool TagManager::saveAllTags()
{
	if (m_tagsFilePath.isEmpty()) {
		return false;
	}

	QFile file(m_tagsFilePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qDebug() << "Cannot save tags file:" << m_tagsFilePath << file.errorString();
		return false;
	}

	QTextStream out(&file);
	QStringList sortedTags = m_allTags.values();
	std::sort(sortedTags.begin(), sortedTags.end());

	for (const QString &tag : sortedTags) {
		out << tag << "\n";
	}

	file.close();
	qDebug() << "Saved" << m_allTags.size() << "tags to" << m_tagsFilePath;
	return true;
}

QSet<QString> TagManager::getObjectTags(const QString& objectPath) 
{
	// Проверяем кэш в памяти
	if (m_objectTags.contains(objectPath)) {
		return m_objectTags[objectPath];
	}

	// Загружаем из ADS (Windows) или файла
	QSet<QString> tags;
	loadTagsFromADS(objectPath, tags);

	// Сохраняем в кэш
	const_cast<QMap<QString, QSet<QString>>&>(m_objectTags)[objectPath] = tags;

	return tags;
}

bool TagManager::setObjectTags(const QString& objectPath, const QSet<QString>& tags)
{
	// Сохраняем в ADS
	if (!saveTagsToADS(objectPath, tags)) {
		return false;
	}

	// Обновляем кэш
	m_objectTags[objectPath] = tags;

	// Добавляем новые теги в общий список
	for (const QString &tag : tags) {
		if (!m_allTags.contains(tag)) {
			m_allTags.insert(tag);
		}
	}

	// Сохраняем обновленный список тегов
	saveAllTags();

	emit tagsChanged(objectPath);
	emit globalTagsChanged();

	return true;
}

void TagManager::addGlobalTag(const QString& tag)
{
	if (!m_allTags.contains(tag)) {
		m_allTags.insert(tag);
		saveAllTags();
		emit globalTagsChanged();
	}
}

bool TagManager::removeGlobalTag(const QString& tag)
{
	if (m_allTags.remove(tag)) {
		saveAllTags();

		// Удаляем этот тег из всех объектов
		for (auto it = m_objectTags.begin(); it != m_objectTags.end(); ++it) {
			it.value().remove(tag);
		}

		emit globalTagsChanged();
		return true;
	}
	return false;
}


bool TagManager::loadTagsFromADS(const QString& filePath, QSet<QString>& tags)
{
#ifdef Q_OS_WINDOWS
	// Windows: пробуем ADS, если не получится - .tags файл
	QString streamPath = filePath + ":tags";
	QFile adsFile(streamPath);

	if (adsFile.exists() && adsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QTextStream in(&adsFile);
		QString content = in.readAll();
		adsFile.close();

		QStringList tagList = content.split(' ', Qt::SkipEmptyParts);
		for (const QString &tag : tagList) {
			tags.insert(tag.trimmed());
		}
		return true;
	}
#endif

	// Универсальный fallback: .tags файл рядом
	QString tagsFilePath = filePath + ".tags";
	QFile file(tagsFilePath);

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}

	QTextStream in(&file);
	QString content = in.readAll();
	file.close();

	QStringList tagList = content.split(' ', Qt::SkipEmptyParts);
	for (const QString &tag : tagList) {
		tags.insert(tag.trimmed());
	}

	return true;
}

bool TagManager::saveTagsToADS(const QString& filePath, const QSet<QString>& tags)
{
	QStringList tagList = tags.values();
	std::sort(tagList.begin(), tagList.end());
	QString content = tagList.join(' ');

#ifdef Q_OS_WINDOWS
	// Windows: пробуем ADS сначала
	QString streamPath = filePath + ":tags";
	QFile adsFile(streamPath);

	if (adsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&adsFile);
		out << content;
		adsFile.close();
		return true;
	}
#endif

	// Универсальный fallback: .tags файл
	QString tagsFilePath = filePath + ".tags";
	QFile file(tagsFilePath);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qDebug() << "Cannot save tags to:" << tagsFilePath << file.errorString();
		return false;
	}

	QTextStream out(&file);
	out << content;
	file.close();

	return true;
}