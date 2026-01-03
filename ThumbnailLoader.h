#pragma once

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QWaitCondition>

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

private:
	QPixmap generateThumbnail(const QString& filePath, const QSize& size);
	QPixmap generateVideoThumbnail(const QString& videoPath, const QSize& size);

	bool abortFlag;
	QMutex mutex;
};
