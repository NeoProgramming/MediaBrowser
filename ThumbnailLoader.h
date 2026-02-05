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


class ThumbnailLoader : public QObject
{
	Q_OBJECT

public:
	explicit ThumbnailLoader(const QString &ffmpeg_path, int tn_size, QObject *parent = nullptr);
	~ThumbnailLoader();

public slots:
	void loadThumbnails(const QString& folderPath);
	void cancelLoading();

signals:
	void thumbnailLoaded(int index, const QPixmap& pixmap);
	void loadingFinished();
	void errorOccurred(const QString& error);

private slots:
//	void onVideoFrameAvailable(const QVideoFrame &frame);

private:
	QPixmap generateImageThumbnail(const QString& imagePath, int size);
	QPixmap generateVideoThumbnail(const QString& videoPath, int size);
	QPixmap extractFrameWithFFmpeg(const QString& videoPath, int size);

//	QPixmap createVideoPlaceholder(const QSize& size, const QString& filename);
//	QPixmap addVideoOverlay(const QPixmap& basePixmap, const QSize& size);


	bool m_abortFlag;
	QMutex m_mutex;
	QString m_ffmpegPath;
	int m_thumbnailSize = 200; // Размер превью

//	bool abortFlag;
//	QMutex mutex;
//	QMediaPlayer *mediaPlayer;
//	QVideoProbe *videoProbe;
//	QPixmap lastVideoFrame;
//	QMutex frameMutex;
//	QWaitCondition frameCaptured;
//	bool frameReady;
};
