#include "ThumbnailLoader.h"
#include <QThread>
#include <QImageReader>
#include <QPainter>
#include <QMediaPlayer>
//#include <QVideoSink>
#include <QVideoFrame>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QTimer>
//#include <QVideoWidget>
#include <QBuffer>
#include <QDebug>
#include "FFmpegThumbnailer.h"

ThumbnailLoader::ThumbnailLoader(QObject *parent)
	: QObject(parent)
	, abortFlag(false)
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
		// Проверяем флаг отмены
		{
			QMutexLocker locker(&mutex);
			if (abortFlag) {
				qDebug() << "Загрузка отменена";
				break;
			}
		}

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
	// Проверяем отмену
	{
		QMutexLocker locker(&mutex);
		if (abortFlag) return QPixmap();
	}

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
	// Проверяем отмену
	{
		QMutexLocker locker(&mutex);
		if (abortFlag) return QPixmap();
	}

	qDebug() << "Генерация превью для видео:" << videoPath;

	// Пробуем FFmpeg сначала (если доступен)
	if (FFmpegThumbnailer::isAvailable()) {
		QPixmap thumbnail = FFmpegThumbnailer::generateThumbnail(videoPath, size);
		if (!thumbnail.isNull()) {
			return addVideoOverlay(thumbnail, size);
		}	
	}

	// Пробуем несколько методов
	QPixmap thumbnail;

	// Метод 1: Используем QMediaPlayer для захвата кадра
	thumbnail = extractFrameFromVideo(videoPath, size);

	// Метод 2: Если не получилось, создаем заглушку
	if (thumbnail.isNull()) {
		thumbnail = createVideoPlaceholder(size, QFileInfo(videoPath).fileName());
	}

	return thumbnail;
/*
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

	return pixmap;*/
}

QPixmap ThumbnailLoader::extractFrameFromVideo(const QString& videoPath, const QSize& size)
{
	// Создаем локальные объекты для захвата кадра
	QMediaPlayer *player = new QMediaPlayer();
	QVideoProbe *probe = new QVideoProbe();
	QEventLoop loop;
	QTimer timeoutTimer;

	QPixmap result;
	bool frameCaptured = false;

	// Настраиваем таймаут
	timeoutTimer.setSingleShot(true);
	timeoutTimer.start(5000); // 5 секунд таймаут

	// Подключаем probe к player
	if (!probe->setSource(player)) {
		qDebug() << "Не удалось подключить VideoProbe";
		delete probe;
		delete player;
		return result;
	}

	// Слот для захвата кадра
	auto frameHandler = [&](const QVideoFrame &frame) {
		if (frame.isValid() && !frameCaptured) {
			QVideoFrame cloneFrame(frame);
			if (cloneFrame.map(QAbstractVideoBuffer::ReadOnly)) {
				QImage image;

				// Конвертируем видеофрейм в QImage
				QVideoFrame::PixelFormat pixelFormat = cloneFrame.pixelFormat();

				if (pixelFormat == QVideoFrame::Format_ARGB32 ||
					pixelFormat == QVideoFrame::Format_RGB32) {
					image = QImage(cloneFrame.bits(),
						cloneFrame.width(),
						cloneFrame.height(),
						cloneFrame.bytesPerLine(),
						QImage::Format_ARGB32);
				}
				else if (pixelFormat == QVideoFrame::Format_RGB24) {
					image = QImage(cloneFrame.bits(),
						cloneFrame.width(),
						cloneFrame.height(),
						cloneFrame.bytesPerLine(),
						QImage::Format_RGB888);
				}
				else if (pixelFormat == QVideoFrame::Format_YUYV) {
					// Конвертируем YUYV в RGB
					image = QImage(cloneFrame.width(), cloneFrame.height(), QImage::Format_RGB32);
					const uchar *bits = cloneFrame.bits();
					for (int y = 0; y < cloneFrame.height(); ++y) {
						QRgb *scanline = (QRgb*)image.scanLine(y);
						for (int x = 0; x < cloneFrame.width(); x += 2) {
							int y0 = bits[0];
							int u = bits[1];
							int y1 = bits[2];
							int v = bits[3];
							bits += 4;

							// Конвертация YUV в RGB (упрощенная)
							int c = y0 - 16;
							int d = u - 128;
							int e = v - 128;

							int r = qBound(0, (298 * c + 409 * e + 128) >> 8, 255);
							int g = qBound(0, (298 * c - 100 * d - 208 * e + 128) >> 8, 255);
							int b = qBound(0, (298 * c + 516 * d + 128) >> 8, 255);

							scanline[x] = qRgb(r, g, b);

							c = y1 - 16;
							r = qBound(0, (298 * c + 409 * e + 128) >> 8, 255);
							g = qBound(0, (298 * c - 100 * d - 208 * e + 128) >> 8, 255);
							b = qBound(0, (298 * c + 516 * d + 128) >> 8, 255);

							scanline[x + 1] = qRgb(r, g, b);
						}
					}
				}

				cloneFrame.unmap();

				if (!image.isNull()) {
					// Масштабируем и создаем превью
					QImage scaled = image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
					result = QPixmap::fromImage(scaled);
					frameCaptured = true;

					// Добавляем индикатор видео
					if (!result.isNull()) {
						QPainter painter(&result);
						painter.setRenderHint(QPainter::Antialiasing);

						// Полупрозрачный черный фон для индикатора
						painter.fillRect(0, 0, result.width(), 25, QColor(0, 0, 0, 180));

						// Иконка воспроизведения
						painter.setPen(Qt::white);
						painter.setBrush(Qt::white);

						QPolygonF triangle;
						triangle << QPointF(10, 7)
							<< QPointF(10, 18)
							<< QPointF(20, 12.5);
						painter.drawPolygon(triangle);

						// Текст
						painter.setFont(QFont("Arial", 9));
						painter.drawText(QRect(30, 0, result.width() - 30, 25),
							Qt::AlignLeft | Qt::AlignVCenter, "Видео");

						// Продолжительность (если можем получить)
						QFileInfo info(videoPath);
						painter.setFont(QFont("Arial", 8));
						painter.drawText(QRect(0, result.height() - 20, result.width(), 20),
							Qt::AlignRight | Qt::AlignBottom,
							info.suffix().toUpper());
					}
				}
			}

			// Выходим из event loop
			if (frameCaptured) {
				loop.quit();
			}
		}
	};

	// Подключаем обработчик кадров
	QObject::connect(probe, &QVideoProbe::videoFrameProbed, frameHandler);

	// Обработка ошибок
	QObject::connect(player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
		[&](QMediaPlayer::Error error) {
		qDebug() << "Ошибка MediaPlayer:" << error << player->errorString();
		if (!frameCaptured) {
			loop.quit();
		}
	});

	// Обработка таймаута
	QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

	// Обработка загрузки медиа
	QObject::connect(player, &QMediaPlayer::mediaStatusChanged,
		[&](QMediaPlayer::MediaStatus status) {
		if (status == QMediaPlayer::LoadedMedia ||
			status == QMediaPlayer::BufferedMedia) {
			// Пытаемся получить кадр в позиции 1 секунда
			player->setPosition(1000);
		}
		else if (status == QMediaPlayer::InvalidMedia) {
			qDebug() << "Неверный медиафайл";
			loop.quit();
		}
	});

	// Устанавливаем видео и запускаем
	player->setMedia(QUrl::fromLocalFile(videoPath));
	player->setVideoOutput((QVideoWidget*)nullptr); // Без видеовыхода

	// Запускаем event loop
	loop.exec();

	// Останавливаем и очищаем
	player->stop();
	player->deleteLater();
	probe->deleteLater();

	// Если не удалось захватить кадр, создаем заглушку
	if (result.isNull()) {
		qDebug() << "Не удалось захватить кадр из видео:" << videoPath;
	}

	return result;
}

// Вспомогательная функция для создания заглушки видео
QPixmap ThumbnailLoader::createVideoPlaceholder(const QSize& size, const QString& filename)
{
	QPixmap pixmap(size);
	pixmap.fill(QColor(50, 50, 60));

	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);

	// Градиентный фон
	QLinearGradient gradient(0, 0, 0, size.height());
	gradient.setColorAt(0, QColor(70, 70, 80));
	gradient.setColorAt(1, QColor(40, 40, 50));
	painter.fillRect(pixmap.rect(), gradient);

	// Иконка видео
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(100, 150, 220));

	// Большая иконка воспроизведения
	int iconSize = qMin(size.width(), size.height()) / 3;
	QRect iconRect((size.width() - iconSize) / 2,
		(size.height() - iconSize) / 2 - 10,
		iconSize, iconSize);

	QPolygonF triangle;
	triangle << QPointF(iconRect.left() + iconSize * 0.3, iconRect.top() + iconSize * 0.2)
		<< QPointF(iconRect.left() + iconSize * 0.3, iconRect.bottom() - iconSize * 0.2)
		<< QPointF(iconRect.right() - iconSize * 0.3, iconRect.top() + iconSize * 0.5);

	painter.drawPolygon(triangle);

	// Название файла
	QString displayName = filename;
	if (displayName.length() > 20) {
		displayName = displayName.left(17) + "...";
	}

	painter.setPen(Qt::white);
	painter.setFont(QFont("Arial", 10, QFont::Bold));

	QRect textRect(0, size.height() - 40, size.width(), 40);
	painter.drawText(textRect, Qt::AlignCenter, displayName);

	// Информация о формате
	painter.setFont(QFont("Arial", 8));
	painter.setPen(QColor(180, 180, 180));

	QString ext = QFileInfo(filename).suffix().toUpper();
	painter.drawText(QRect(0, size.height() - 20, size.width(), 20),
		Qt::AlignCenter, ext + " Video");

	// Рамка
	painter.setPen(QPen(QColor(100, 100, 120), 2));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(pixmap.rect().adjusted(1, 1, -1, -1));

	return pixmap;
}


void ThumbnailLoader::cancelLoading()
{
	QMutexLocker locker(&mutex);
	abortFlag = true;
}

// Функция для добавления оверлея видео на превью
QPixmap ThumbnailLoader::addVideoOverlay(const QPixmap& basePixmap, const QSize& size)
{
	if (basePixmap.isNull()) {
		return basePixmap;
	}

	QPixmap result = basePixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	// Если изображение меньше нужного размера, центрируем его
	if (result.width() < size.width() || result.height() < size.height()) {
		QPixmap centered(size);
		centered.fill(Qt::black); // Черный фон

		QPainter painter(&centered);
		painter.setRenderHint(QPainter::Antialiasing);

		// Центрируем изображение
		int x = (size.width() - result.width()) / 2;
		int y = (size.height() - result.height()) / 2;
		painter.drawPixmap(x, y, result);

		result = centered;
	}

	// Добавляем оверлей с информацией о видео
	QPainter painter(&result);
	painter.setRenderHint(QPainter::Antialiasing);

	// 1. Индикатор в верхнем левом углу
	QRect infoRect(5, 5, result.width() / 3, 25);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0, 0, 0, 180)); // Полупрозрачный черный
	painter.drawRoundedRect(infoRect, 5, 5);

	// Иконка воспроизведения
	painter.setPen(Qt::white);
	painter.setBrush(Qt::white);

	QPolygonF triangle;
	triangle << QPointF(infoRect.left() + 7, infoRect.top() + 8)
		<< QPointF(infoRect.left() + 7, infoRect.bottom() - 8)
		<< QPointF(infoRect.left() + 17, infoRect.top() + infoRect.height() / 2);
	painter.drawPolygon(triangle);

	// Текст "VIDEO"
	painter.setFont(QFont("Arial", 9, QFont::Bold));
	painter.drawText(QRect(infoRect.left() + 22, infoRect.top(),
		infoRect.width() - 22, infoRect.height()),
		Qt::AlignLeft | Qt::AlignVCenter, "VIDEO");

	// 2. Полоска времени внизу (опционально)
	QRect timeRect(0, result.height() - 4, result.width(), 4);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(70, 130, 220, 200));
	painter.drawRect(timeRect);

	// 3. Индикатор длительности в правом нижнем углу (если известна длительность)
	// Для простоты показываем иконку камеры
	QRect durationRect(result.width() - 35, result.height() - 30, 30, 25);
	painter.setBrush(QColor(0, 0, 0, 150));
	painter.drawRoundedRect(durationRect, 3, 3);

	// Иконка камеры
	painter.setPen(QPen(Qt::white, 1));
	painter.setBrush(Qt::NoBrush);

	// Объектив камеры
	painter.drawEllipse(QPoint(durationRect.center().x(), durationRect.center().y() - 2), 6, 6);

	// Корпус камеры
	painter.drawRect(durationRect.left() + 8, durationRect.top() + 5, 14, 8);

	return result;
}