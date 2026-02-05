#pragma once
#include <QString>

struct Settings
{
	void loadSettings();
	void saveSettings();

	QString ffmpegPath;
	QString tagsPath;
	QString sourceRoot;
	QString targetRoot;
	int thumbnailSize;
	int thumbnailColumns;
};

