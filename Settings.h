#pragma once
#include <QString>

struct Settings
{
	// Константы для значений по умолчанию
	static const int DEFAULT_LEFT_PANEL_WIDTH = 300;
	static const int DEFAULT_RIGHT_PANEL_WIDTH = 350; // Увеличили с 250 до 350
	static const int DEFAULT_THUMBNAIL_SIZE = 200;
	static const int DEFAULT_THUMBNAIL_COLUMNS = 4;

	void loadSettings();
	void saveSettings();

	QString ffmpegPath;
	QString tagsPath;
	QString sourceRoot;
	QString targetRoot;
	int thumbnailSize;
	int thumbnailColumns;

	QByteArray windowGeometry;
	QByteArray windowState;
	int leftPanelWidth;
	int rightPanelWidth;
};

