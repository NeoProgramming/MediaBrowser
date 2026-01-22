// ffmpegthumbnailer.cpp
#include "ffmpegthumbnailer.h"
#include <QProcess>
#include <QBuffer>
#include <QDebug>
#include <QTemporaryFile>
#include <QDir>

#ifdef Q_OS_WINDOWS
#include <windows.h>
#endif

const char *ffmpegPath = "e:/MySoft/_Video/ffmpeg/bin/ffmpeg.exe";

QPixmap FFmpegThumbnailer::generateThumbnail(const QString& videoPath, const QSize& size)
{
    if (!isAvailable()) {
        return QPixmap();
    }
    
    QProcess ffmpeg;
    QStringList arguments;
    
    // Создаем временный файл для превью
    QTemporaryFile tempFile(QDir::tempPath() + "/video_thumb_XXXXXX.jpg");
    if (!tempFile.open()) {
        qDebug() << "Не удалось создать временный файл";
        return QPixmap();
    }
    QString tempFilePath = tempFile.fileName();
    tempFile.close();
    
    // Аргументы ffmpeg для захвата кадра на 1 секунде
    arguments << "-i" << videoPath
              << "-ss" << "00:00:01"  // Время: 1 секунда
              << "-vframes" << "1"     // Только один кадр
              << "-vf" << QString("scale=%1:%2:force_original_aspect_ratio=decrease,pad=%1:%2:(ow-iw)/2:(oh-ih)/2")
                            .arg(size.width())
                            .arg(size.height())
              << "-q:v" << "2"         // Качество JPEG (2-31, где 2 - лучшее)
              << "-y"                  // Перезаписать выходной файл
              << tempFilePath;
    
    ffmpeg.start(ffmpegPath, arguments);
    
    if (!ffmpeg.waitForStarted()) {
        qDebug() << "Не удалось запустить ffmpeg";
        return QPixmap();
    }
    
    // Ждем завершения (максимум 10 секунд)
    if (!ffmpeg.waitForFinished(10000)) {
        ffmpeg.kill();
        qDebug() << "ffmpeg таймаут";
        return QPixmap();
    }
    
    if (ffmpeg.exitCode() != 0) {
        qDebug() << "ffmpeg ошибка:" << ffmpeg.readAllStandardError();
        return QPixmap();
    }
    
    // Загружаем созданное превью
    QPixmap pixmap(tempFilePath);
    
    // Удаляем временный файл
    QFile::remove(tempFilePath);
    
    return pixmap;
}

bool FFmpegThumbnailer::isAvailable()
{
    QProcess ffmpeg;
    ffmpeg.start(ffmpegPath, QStringList() << "-version");
    
    if (!ffmpeg.waitForStarted()) {
        return false;
    }
    
    if (!ffmpeg.waitForFinished(3000)) {
        ffmpeg.kill();
        return false;
    }
    
    return (ffmpeg.exitCode() == 0);
}