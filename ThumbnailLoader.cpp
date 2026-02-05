#include "ThumbnailLoader.h"
#include <QThread>
#include <QImageReader>
#include <QPainter>
#include <QMediaPlayer>
#include <QVideoFrame>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QBuffer>
#include <QDebug>
#include <QProcess>
#include "FFmpegThumbnailer.h"

ThumbnailLoader::ThumbnailLoader(const QString &ffmpeg_path, int tn_size, QObject *parent)
	: QObject(parent)
	, m_ffmpegPath(ffmpeg_path)
	, m_thumbnailSize(tn_size)
	, m_abortFlag(false)
{
}

ThumbnailLoader::~ThumbnailLoader()
{
	cancelLoading();
}

void ThumbnailLoader::loadThumbnails(const QString& folderPath)
{
	QMutexLocker locker(&m_mutex);
	m_abortFlag = false;
	
	locker.unlock();

	QDir dir(folderPath);

	// Фильтры для файлов
	QStringList imageFilters;
	imageFilters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif"
		<< "*.tiff" << "*.webp" << "*.JPG" << "*.JPEG" << "*.PNG";

	QStringList videoFilters;
	videoFilters << "*.mp4" << "*.avi" << "*.mkv" << "*.mov" << "*.wmv"
		<< "*.flv" << "*.m4v" << "*.mpg" << "*.mpeg" << "*.3gp"
		<< "*.MP4" << "*.AVI" << "*.MKV" << "*.MOV";

	QStringList allFilters = imageFilters + videoFilters;
	QStringList files = dir.entryList(allFilters, QDir::Files, QDir::Name);
		

	for (int i = 0; i < files.size(); ++i) {
		// Проверяем отмену
		{
			QMutexLocker locker(&m_mutex);
			if (m_abortFlag) break;
		}

		QString filePath = dir.absoluteFilePath(files[i]);
		QPixmap thumbnail;

		QFileInfo fileInfo(filePath);
		QString suffix = fileInfo.suffix().toLower();

		// Определяем тип файла
		bool isVideo = videoFilters.contains("*." + suffix, Qt::CaseInsensitive);

		if (isVideo) {
			thumbnail = generateVideoThumbnail(filePath, m_thumbnailSize);
		}
		else {
			thumbnail = generateImageThumbnail(filePath, m_thumbnailSize);
		}

		if (!thumbnail.isNull()) {
			emit thumbnailLoaded(i, thumbnail);
		}

		// Даем возможность обработать события
		QThread::msleep(1);
	}

	emit loadingFinished();
}

QPixmap ThumbnailLoader::generateImageThumbnail(const QString& filePath, int size)
{
	QMutexLocker locker(&m_mutex);
	if (m_abortFlag) return QPixmap();
	locker.unlock();

	QImageReader reader(filePath);
	if (!reader.canRead()) {
		return QPixmap();
	}

	reader.setAutoTransform(true); // Автоповорот по EXIF

	// Читаем с уменьшением для скорости
	QSize imageSize = reader.size();
	if (imageSize.width() > 800 || imageSize.height() > 800) {
		imageSize.scale(800, 800, Qt::KeepAspectRatio);
		reader.setScaledSize(imageSize);
	}

	QImage image = reader.read();
	if (image.isNull()) {
		return QPixmap();
	}

	QImage scaled = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	return QPixmap::fromImage(scaled);
}

QPixmap ThumbnailLoader::generateVideoThumbnail(const QString& filePath, int size)
{
	QMutexLocker locker(&m_mutex);
	if (m_abortFlag) return QPixmap();
	locker.unlock();

	QPixmap thumbnail = extractFrameWithFFmpeg(filePath, size);

	// Если не получилось, создаем заглушку
	if (thumbnail.isNull()) {
		thumbnail = QPixmap(size, size);
		thumbnail.fill(QColor(50, 50, 60));

		QPainter painter(&thumbnail);
		painter.setRenderHint(QPainter::Antialiasing);

		// Иконка видео
		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor(100, 150, 220));

		QPolygonF triangle;
		triangle << QPointF(size * 0.3, size * 0.2)
			<< QPointF(size * 0.3, size * 0.8)
			<< QPointF(size * 0.7, size * 0.5);

		painter.drawPolygon(triangle);

		painter.setPen(Qt::white);
		painter.setFont(QFont("Arial", 10));
		painter.drawText(thumbnail.rect(), Qt::AlignBottom | Qt::AlignHCenter,
			QFileInfo(filePath).suffix().toUpper());
	}

	return thumbnail;
}

QPixmap ThumbnailLoader::extractFrameWithFFmpeg(const QString& videoPath, int size)
{
	QProcess ffmpeg;
	QByteArray outputData;

	// Проверяем существование ffmpeg
	QFileInfo ffmpegInfo(m_ffmpegPath);
	if (!ffmpegInfo.exists() || !ffmpegInfo.isFile()) {
		emit errorOccurred(QString("FFmpeg not found: %1").arg(m_ffmpegPath));
		return QPixmap();
	}

	QStringList args;
	args << "-i" << videoPath
		<< "-ss" << "1"                     // 1 секунда от начала
		<< "-vframes" << "1"                // Только один кадр
		<< "-vf" << QString("scale=%1:%2:force_original_aspect_ratio=decrease")
		.arg(size).arg(size)
		<< "-f" << "image2pipe"            // Вывод в pipe
		<< "-c:v" << "png"                 // Используем PNG
		<< "-" << "-y";                    // "-" означает вывод в stdout

	ffmpeg.start(m_ffmpegPath, args);

	if (!ffmpeg.waitForStarted(2000)) {
		emit errorOccurred("Failed to start FFmpeg");
		return QPixmap();
	}

	// Читаем все данные
	if (!ffmpeg.waitForFinished(5000)) {
		ffmpeg.kill();
		emit errorOccurred("FFmpeg timeout");
		return QPixmap();
	}

	if (ffmpeg.exitCode() != 0) {
		QString error = ffmpeg.readAllStandardError();
		emit errorOccurred(QString("FFmpeg error: %1").arg(error));
		return QPixmap();
	}

	outputData = ffmpeg.readAllStandardOutput();

	if (outputData.isEmpty()) {
		emit errorOccurred("FFmpeg returned no data.");
		return QPixmap();
	}

	QPixmap pixmap;
	if (!pixmap.loadFromData(outputData, "PNG")) {
		emit errorOccurred("Failed to load image from FFmpeg data");
		return QPixmap();
	}

	return pixmap;
}

void ThumbnailLoader::cancelLoading()
{
	QMutexLocker locker(&m_mutex);
	m_abortFlag = true;
}
