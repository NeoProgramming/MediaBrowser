#include "Settings.h"
#include <QSettings>

static const char INI_FILE[] = "mediabrowser.ini";

#define SETTINGS_LIST	\
	X(m_ffmpegPath,			"ffmpeg", "ffmpeg.exe")	\
	X(m_tagsPath,			"tags",	  "tags.txt")	\
	X(m_lastSourceFolder,	"source", ".")	\
	X(m_lastTargetFolder,	"target", ".")	\
	X(m_thumbnailSize,		"size",   "200")\
	X(m_thumbnailColumns,	"columns","4")

// Qt does not QVariant.getValue() method
template<typename T>
void getValue(const QVariant &v, T &x)
{
	x = v.value<T>();
}

void Settings::loadSettings()
{
	QSettings settings(INI_FILE, QSettings::IniFormat);
	
#define X(var, id, def)	getValue(settings.value(id, def), var);
	SETTINGS_LIST
#undef X

	if (settings.allKeys().size() != 6)
		saveSettings();
}

void Settings::saveSettings()
{
	QSettings settings(INI_FILE, QSettings::IniFormat);

#define X(var, id, def)	settings.setValue(id, var);
	SETTINGS_LIST
#undef X
}

