#pragma once
#include <QList>
#include <QStringList>

// Вспомогательный метод для получения информации о выделенных файлах
struct SelectedFilesInfo {
	QStringList filenames;      // Имена файлов
	QList<int> indices;         // Их индексы (отсортированные по убыванию для удаления)
	bool isEmpty() const { return filenames.isEmpty(); }
	int size() const { return filenames.size(); }
};


