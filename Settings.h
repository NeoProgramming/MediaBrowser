#pragma once
#include <QString>

struct Settings
{
	void loadSettings();
	void saveSettings();

	QString m_ffmpegPath;
	QString m_tagsPath;
	QString m_lastSourceFolder;
	QString m_lastTargetFolder;
	int m_thumbnailSize;
	int m_thumbnailColumns;
};

