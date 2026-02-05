#pragma once
#include <QString>

struct Settings
{
	void loadSettings();
	void saveSettings();

	QString ffmpegPath;
	QString tagsPath;
	QString sourceFolder;
	QString targetFolder;
	int thumbnailSize;
	int thumbnailColumns;
};

