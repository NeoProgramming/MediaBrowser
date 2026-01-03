#pragma once

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QWaitCondition>
#include <QMediaPlayer>
#include <QVideoProbe>
//#include <QVideoSink>

class ThumbnailLoader : public QObject
{
	Q_OBJECT

public:
	explicit ThumbnailLoader(QObject *parent = nullptr);
	~ThumbnailLoader();

public slots:
	void loadThumbnails(const QString& folderPath);
	void cancelLoading();

signals:
	void thumbnailLoaded(int index, const QPixmap& pixmap);
	void loadingFinished();
private slots:
//	void onVideoFrameAvailable(const QVideoFrame &frame);

private:
	QPixmap generateThumbnail(const QString& filePath, const QSize& size);
	QPixmap generateVideoThumbnail(const QString& videoPath, const QSize& size);
	QPixmap extractFrameFromVideo(const QString& videoPath, const QSize& size);
	QPixmap createVideoPlaceholder(const QSize& size, const QString& filename);
	QPixmap addVideoOverlay(const QPixmap& basePixmap, const QSize& size);

	bool abortFlag;
	QMutex mutex;

	// Для захвата кадров видео
	QMediaPlayer *mediaPlayer;
	QVideoProbe *videoProbe;
	QPixmap lastVideoFrame;
	QMutex frameMutex;
	QWaitCondition frameCaptured;
	bool frameReady;
};
