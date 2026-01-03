#include "mediabrowser.h"
#include "thumbnailloader.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QLabel>
#include <QScrollArea>
#include <QGridLayout>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QMessageBox>

MediaBrowser::MediaBrowser(QWidget *parent)
    : QMainWindow(parent)
{
    //ui.setupUi(this);
	setupUI();
	setupMenu();
	initThumbnailLoader();

//	thumbnailLoader = new ThumbnailLoader();
//	loaderThread = new QThread();
//	thumbnailLoader->moveToThread(loaderThread);
//	connect(loaderThread, &QThread::finished, thumbnailLoader, &QObject::deleteLater);
//	connect(this, &MediaBrowser::destroyed, this, [this]() {
//		thumbnailLoader->cancelLoading();
//		loaderThread->quit();
//		loaderThread->wait();
//	});
//	connect(thumbnailLoader, &ThumbnailLoader::thumbnailLoaded,
//		this, &MediaBrowser::onThumbnailLoaded);
//	loaderThread->start();
}

MediaBrowser::~MediaBrowser()
{
	// Отменяем загрузку и завершаем поток ПЕРЕД удалением
	if (thumbnailLoader && loaderThread) {
		thumbnailLoader->cancelLoading();
		loaderThread->quit();
		loaderThread->wait(1000); // Ждем завершения, но не бесконечно
	}

	delete loaderThread; // Удаляем поток
	// thumbnailLoader удалится автоматически через parent или thread
}

void MediaBrowser::initThumbnailLoader()
{
	thumbnailLoader = new ThumbnailLoader();
	loaderThread = new QThread(this); // Указываем parent для автоматического удаления

	thumbnailLoader->moveToThread(loaderThread);

	// Подключаем сигналы
	connect(loaderThread, &QThread::finished, thumbnailLoader, &QObject::deleteLater);
	connect(thumbnailLoader, &ThumbnailLoader::thumbnailLoaded,
		this, &MediaBrowser::onThumbnailLoaded);
	connect(thumbnailLoader, &ThumbnailLoader::loadingFinished,
		this, [this]() {
		statusBar()->showMessage("Loading finished", 3000);
	});

	loaderThread->start();
}


void MediaBrowser::setupUI()
{
	resize(1024, 768);

	centralWidget = new QWidget(this);
	setCentralWidget(centralWidget);

	scrollArea = new QScrollArea(centralWidget);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	thumbnailContainer = new QWidget();
	thumbnailLayout = new QGridLayout(thumbnailContainer);
	thumbnailLayout->setSpacing(10);
	thumbnailLayout->setContentsMargins(10, 10, 10, 10);

	scrollArea->setWidget(thumbnailContainer);

	QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
	mainLayout->addWidget(scrollArea);
	mainLayout->setContentsMargins(0, 0, 0, 0);
}

void MediaBrowser::setupMenu()
{
	QMenu *fileMenu = menuBar()->addMenu("File");

	QAction *openAction = fileMenu->addAction("Open dir");
	openAction->setShortcut(QKeySequence::Open);
	connect(openAction, &QAction::triggered, this, &MediaBrowser::selectFolder);

	QAction *exitAction = fileMenu->addAction("Exit");
	exitAction->setShortcut(QKeySequence::Quit);
	connect(exitAction, &QAction::triggered, this, &QWidget::close);

	QToolBar *toolBar = addToolBar("Main");
	toolBar->addAction(openAction);
}

void MediaBrowser::selectFolder()
{
	QString folder = QFileDialog::getExistingDirectory(this, "Select folder",
		QDir::homePath(),
		QFileDialog::ShowDirsOnly);
	if (!folder.isEmpty()) {
		loadThumbnails(folder);
	}
}

void MediaBrowser::loadThumbnails(const QString& folderPath)
{
	// Очищаем старые превью
	qDeleteAll(thumbnailLabels);
	thumbnailLabels.clear();
	currentFiles.clear();

	currentFolder = folderPath;
	setWindowTitle("View - " + QFileInfo(folderPath).fileName());

	// Получаем список файлов
	QDir dir(folderPath);
	QStringList filters;
	filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif"
		<< "*.tiff" << "*.webp" << "*.mp4" << "*.avi" << "*.mkv"
		<< "*.mov" << "*.wmv";

	currentFiles = dir.entryList(filters, QDir::Files, QDir::Name).toVector();

	// Создаем контейнеры для превью
	const int columns = 4;
	for (int i = 0; i < currentFiles.size(); ++i) {
		QLabel *thumbnailLabel = new QLabel(thumbnailContainer);
		thumbnailLabel->setFixedSize(220, 220);
		thumbnailLabel->setAlignment(Qt::AlignCenter);
		thumbnailLabel->setStyleSheet("border: 1px solid #ddd; background: white;");

		// Временный плейсхолдер
		thumbnailLabel->setText("Loading...");

		// Делаем кликабельным
		thumbnailLabel->setCursor(Qt::PointingHandCursor);
		thumbnailLabel->installEventFilter(this);
		thumbnailLabel->setProperty("fileIndex", i);

		int row = i / columns;
		int col = i % columns;

		thumbnailLayout->addWidget(thumbnailLabel, row, col);
		thumbnailLabels.append(thumbnailLabel);
	}

	// Запускаем загрузку превью в отдельном потоке
	QMetaObject::invokeMethod(thumbnailLoader, "loadThumbnails",
		Q_ARG(QString, folderPath));
}

void MediaBrowser::onThumbnailLoaded(int index, const QPixmap& pixmap)
{
	if (index >= 0 && index < thumbnailLabels.size()) {
		QLabel *label = thumbnailLabels[index];
		label->setPixmap(pixmap);
		label->setText("");

		// Добавляем подпись с именем файла
		QString fileName = currentFiles[index];
		if (fileName.length() > 20) {
			fileName = fileName.left(17) + "...";
		}

		QString toolTip = QString("<b>%1</b><br>Size: %2x%3")
			.arg(currentFiles[index])
			.arg(pixmap.width())
			.arg(pixmap.height());
		label->setToolTip(toolTip);
	}
}

bool MediaBrowser::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonDblClick) {
		QLabel *label = qobject_cast<QLabel*>(obj);
		if (label) {
			int index = label->property("fileIndex").toInt();
			if (index >= 0 && index < currentFiles.size()) {
				QString filePath = currentFolder + "/" + currentFiles[index];
				QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
				return true;
			}
		}
	}
	return QMainWindow::eventFilter(obj, event);
}

void MediaBrowser::onItemDoubleClicked(const QModelIndex& index)
{
	// Реализация открытия файла
}

void MediaBrowser::closeEvent(QCloseEvent *event)
{
	// Отменяем загрузку при закрытии окна
	if (thumbnailLoader) {
		thumbnailLoader->cancelLoading();
	}

	QMainWindow::closeEvent(event);
}