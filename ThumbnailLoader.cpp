#include "ThumbnailLoader.h"
#include <QThread>
#include <QImageReader>
#include <QPainter>
#include <QMediaPlayer>
//#include <QVideoSink>
#include <QVideoFrame>

ThumbnailLoader::ThumbnailLoader(QObject *parent)
	: QObject(parent)
{
}

ThumbnailLoader::~ThumbnailLoader()
{
	cancelLoading();
}

void ThumbnailLoader::loadThumbnails(const QString& folderPath)
{
	abortFlag = false;

	QDir dir(folderPath);
	QStringList filters;
	filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif"
		<< "*.tiff" << "*.webp" << "*.mp4" << "*.avi" << "*.mkv"
		<< "*.mov" << "*.wmv";

	QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

	const int thumbnailSize = 200;
	QSize size(thumbnailSize, thumbnailSize);

	for (int i = 0; i < files.size(); ++i) {
		if (abortFlag) break;

		QString filePath = dir.absoluteFilePath(files[i]);
		QPixmap thumbnail;

		QFileInfo fileInfo(filePath);
		QString suffix = fileInfo.suffix().toLower();

		if (suffix == "mp4" || suffix == "avi" || suffix == "mkv" ||
			suffix == "mov" || suffix == "wmv") {
			thumbnail = generateVideoThumbnail(filePath, size);
		}
		else {
			thumbnail = generateThumbnail(filePath, size);
		}

		if (!thumbnail.isNull()) {
			emit thumbnailLoaded(i, thumbnail);
		}

		// Для предотвращения блокировки UI
		QThread::msleep(10);
	}

	emit loadingFinished();
}

QPixmap ThumbnailLoader::generateThumbnail(const QString& filePath, const QSize& size)
{
	QImage image(filePath);
	if (image.isNull()) {
		return QPixmap();
	}

	QImage scaled = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	// Создаем превью с рамкой
	QPixmap pixmap(size);
	pixmap.fill(Qt::white);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);

	// Центрируем изображение
	int x = (size.width() - scaled.width()) / 2;
	int y = (size.height() - scaled.height()) / 2;

	painter.drawImage(x, y, scaled);

	// Рисуем рамку
	painter.setPen(QPen(Qt::lightGray, 1));
	painter.drawRect(0, 0, size.width() - 1, size.height() - 1);

	return pixmap;
}

QPixmap ThumbnailLoader::generateVideoThumbnail(const QString& videoPath, const QSize& size)
{
	// Для видео можно использовать QMediaPlayer для получения кадра
	// В реальном приложении может потребоваться FFmpeg
	QPixmap pixmap(size);
	pixmap.fill(Qt::darkGray);

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);

	// Рисуем иконку видео
	painter.setPen(Qt::white);
	painter.setFont(QFont("Arial", 12));
	painter.drawText(pixmap.rect(), Qt::AlignCenter, "VIDEO");

	// Рисуем рамку
	painter.setPen(QPen(Qt::lightGray, 1));
	painter.drawRect(0, 0, size.width() - 1, size.height() - 1);

	return pixmap;
}

void ThumbnailLoader::cancelLoading()
{
	QMutexLocker locker(&mutex);
	abortFlag = true;

}
