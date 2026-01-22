// ffmpegthumbnailer.h
#ifndef FFMPEGTHUMBNAILER_H
#define FFMPEGTHUMBNAILER_H

#include <QPixmap>
#include <QSize>

class FFmpegThumbnailer
{
public:
    static QPixmap generateThumbnail(const QString& videoPath, const QSize& size);
    static bool isAvailable();
};

#endif