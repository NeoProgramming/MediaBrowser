#include "mediabrowser.h"
#include "thumbnailloader.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QGridLayout>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QMessageBox>
#include <QGroupBox>

MediaBrowser::MediaBrowser(QWidget *parent)
    : QMainWindow(parent)
{
	cfg.loadSettings();
	setupUI();
	setupMenu();
	initThumbnailLoader();

	QTimer::singleShot(100, this, &MediaBrowser::loadNextUnprocessedDir);
}

MediaBrowser::~MediaBrowser()
{
	cfg.saveSettings();
	// Отменяем загрузку и завершаем поток ПЕРЕД удалением
	if (thumbnailLoader && loaderThread) {
		thumbnailLoader->cancelLoading();
		loaderThread->quit();
		loaderThread->wait(1000); // Ждем завершения, но не бесконечно
	}

	delete loaderThread; // Удаляем поток
	// thumbnailLoader удалится автоматически через parent или thread
}

void MediaBrowser::loadNextUnprocessedDir()
{

}

void MediaBrowser::initThumbnailLoader()
{
	thumbnailLoader = new ThumbnailLoader(cfg.ffmpegPath, cfg.thumbnailSize);
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

	// Основной горизонтальный layout
	QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

	// Левая панель - дерево целевых папок
	QGroupBox *targetGroup = new QGroupBox("Целевые папки");
	QVBoxLayout *targetLayout = new QVBoxLayout(targetGroup);

	targetTree = new QTreeWidget();
	targetTree->setHeaderLabel("Категории");
	targetLayout->addWidget(targetTree);

	// Поле для выбора целевой папки
	QHBoxLayout *targetPathLayout = new QHBoxLayout();
	targetPathLayout->addWidget(new QLabel("Целевая:"));
	targetPathEdit = new QLineEdit();
	targetPathEdit->setReadOnly(true);
	targetPathLayout->addWidget(targetPathEdit);
	QPushButton *targetBrowseBtn = new QPushButton("...");
	targetBrowseBtn->setFixedWidth(30);
	connect(targetBrowseBtn, &QPushButton::clicked,	this, &MediaBrowser::selectTargetFolder);
	targetPathLayout->addWidget(targetBrowseBtn);
	targetLayout->addLayout(targetPathLayout);

	// Правая панель - превью
	QWidget *rightPanel = new QWidget();
	QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

	// Панель с исходной папкой
	QHBoxLayout *sourceLayout = new QHBoxLayout();
	sourceLayout->addWidget(new QLabel("Исходная:"));
	sourcePathEdit = new QLineEdit();
	sourcePathEdit->setReadOnly(true);
	sourceLayout->addWidget(sourcePathEdit);
	QPushButton *sourceBrowseBtn = new QPushButton("...");
	sourceBrowseBtn->setFixedWidth(30);
	connect(sourceBrowseBtn, &QPushButton::clicked,
		this, &MediaBrowser::selectSourceFolder);
	sourceLayout->addWidget(sourceBrowseBtn);

	rightLayout->addLayout(sourceLayout);

	// Область превью с прокруткой
	scrollArea = new QScrollArea();// centralWidget);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	thumbnailContainer = new QWidget();
	thumbnailLayout = new QGridLayout(thumbnailContainer);
	thumbnailLayout->setSpacing(10);
	thumbnailLayout->setContentsMargins(10, 10, 10, 10);

	scrollArea->setWidget(thumbnailContainer);

	rightLayout->addWidget(scrollArea);

	// Кнопка перемещения
	moveButton = new QPushButton("Переместить папку (Ctrl+M)");
	moveButton->setMinimumHeight(40);
	moveButton->setEnabled(false);
	connect(moveButton, &QPushButton::clicked, this, &MediaBrowser::moveCurrentFolder);
	rightLayout->addWidget(moveButton);

	// Статус FFmpeg
	ffmpegStatusLabel = new QLabel();
	ffmpegStatusLabel->setStyleSheet("color: red;");
	rightLayout->addWidget(ffmpegStatusLabel);

	// Добавляем панели в основной layout
	mainLayout->addWidget(targetGroup, 1); // Левая панель - 1 часть
	mainLayout->addWidget(rightPanel, 3);  // Правая панель - 3 части


	
//	mainLayout->addWidget(scrollArea);
//	mainLayout->setContentsMargins(0, 0, 0, 0);
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


void MediaBrowser::selectSourceFolder()
{
	QString folder = QFileDialog::getExistingDirectory(
		this,
		"Выберите исходную папку",
		cfg.sourceFolder
	);

	if (!folder.isEmpty()) {
		cfg.sourceFolder = folder;
		cfg.saveSettings();
	//	updateSettingsUI();
		loadFolder(folder);
	}
}

void MediaBrowser::selectTargetFolder()
{
	QString folder = QFileDialog::getExistingDirectory(
		this,
		"Выберите целевую папку",
		cfg.targetFolder
	);

	if (!folder.isEmpty()) {
		cfg.targetFolder = folder;
		cfg.saveSettings();
	//	updateSettingsUI();
	}
}

void MediaBrowser::loadThumbnails(const QString& folderPath)
{
	// Очищаем старые превью
	qDeleteAll(thumbnailLabels);
	thumbnailLabels.clear();
	currentFiles.clear();

	cfg.sourceFolder = folderPath;
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

void MediaBrowser::clearThumbnails()
{
	// Удаляем все превью
	for (QLabel *label : thumbnailLabels) {
		thumbnailLayout->removeWidget(label);
		delete label;
	}
	thumbnailLabels.clear();
	currentFiles.clear();

	// Очищаем layout
	while (thumbnailLayout->count() > 0) {
		QLayoutItem *item = thumbnailLayout->takeAt(0);
		if (item->widget()) {
			delete item->widget();
		}
		delete item;
	}
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

void MediaBrowser::onThumbnailsFinished()
{
	statusBar()->showMessage("Загрузка завершена", 3000);
}

bool MediaBrowser::eventFilter(QObject *obj, QEvent *event)
{
	// Реализация открытия файла
	if (event->type() == QEvent::MouseButtonDblClick) {
		QLabel *label = qobject_cast<QLabel*>(obj);
		if (label) {
			int index = label->property("fileIndex").toInt();
			if (index >= 0 && index < currentFiles.size()) {
				QString filePath = cfg.sourceFolder + "/" + currentFiles[index];
				QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
				return true;
			}
		}
	}

	// Обработка наведения мыши для подсветки
	if (event->type() == QEvent::Enter) {
		QLabel *label = qobject_cast<QLabel*>(obj);
		if (label) {
			label->setStyleSheet(
				"border: 2px solid #4CAF50; background: white;"
				"padding: 5px;"
			);
		}
	}

	if (event->type() == QEvent::Leave) {
		QLabel *label = qobject_cast<QLabel*>(obj);
		if (label) {
			label->setStyleSheet(
				"border: 1px solid #ddd; background: white;"
				"padding: 5px;"
			);
		}
	}

	return QMainWindow::eventFilter(obj, event);
}

void MediaBrowser::closeEvent(QCloseEvent *event)
{
	// Отменяем загрузку при закрытии окна
	if (thumbnailLoader) {
		thumbnailLoader->cancelLoading();
	}

	QMainWindow::closeEvent(event);
}

QString MediaBrowser::findNextUnprocessedDir()
{
	if (cfg.sourceFolder.isEmpty()) return QString();

	// Ищем ЛЮБую папку файл в папке
	QDirIterator it(cfg.sourceFolder,
		QStringList() << "*.*",
		QDir::Dirs,
		QDirIterator::NoIteratorFlags);

	if (it.hasNext()) {
		return it.next();
	}

	return QString(); // папок не найдено
}

void MediaBrowser::moveCurrentFolder()
{
	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Ошибка", "Нет текущей папки для перемещения");
		return;
	}

	if (cfg.targetFolder.isEmpty() || !QDir(cfg.targetFolder).exists()) {
		QMessageBox::warning(this, "Ошибка", "Целевая папка не выбрана или не существует");
		return;
	}

	// Получаем имя папки
	QString folderName = QFileInfo(currentFolder).fileName();
	QString targetPath = QDir(cfg.targetFolder).absoluteFilePath(folderName);

	// Проверяем, не существует ли уже такая папка
	if (QDir(targetPath).exists()) {
		int result = QMessageBox::question(this, "Подтверждение",
			QString("Папка '%1' уже существует в целевой директории.\nПерезаписать?")
			.arg(folderName),
			QMessageBox::Yes | QMessageBox::No);

		if (result == QMessageBox::No) {
			return;
		}
	}

	// Перемещаем папку
	QDir dir;
	if (!dir.rename(currentFolder, targetPath)) {
		QMessageBox::critical(this, "Ошибка",
			QString("Не удалось переместить папку '%1' в '%2'")
			.arg(currentFolder).arg(targetPath));
		return;
	}

	statusBar()->showMessage(QString("Папка перемещена в %1").arg(targetPath), 5000);

	// Очищаем и загружаем следующую папку
	clearThumbnails();
	moveButton->setEnabled(false);

	// Ищем следующую папку в исходной директории
	QDir sourceDir(cfg.sourceFolder);
	QStringList subdirs = sourceDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

	if (!subdirs.isEmpty()) {
		QString nextFolder = QDir(cfg.sourceFolder).absoluteFilePath(subdirs.first());
		loadFolder(nextFolder);
	}
	else {
		currentFolder.clear();
		sourcePathEdit->clear();
		QMessageBox::information(this, "Завершено",
			"Все папки в исходной директории обработаны.");
	}
}
