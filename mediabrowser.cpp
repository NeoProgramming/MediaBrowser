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
#include <QDockWidget>

MediaBrowser::MediaBrowser(QWidget *parent)
    : QMainWindow(parent)
	, categoriesModel(nullptr)
	, previewScrollArea(nullptr)
	, previewContainer(nullptr)
	, previewLayout(nullptr)
	, thumbnailLoader(nullptr)
	, loaderThread(nullptr)
{
	// Загружаем настройки
	cfg.loadSettings();

	// Инициализируем UI компоненты
	initPreviewArea();
	initSidebar();
	initMenu();

	// Загружаем первую папку через таймер (после инициализации UI)
	QTimer::singleShot(100, this, &MediaBrowser::loadNextUnprocessedFolder);
}

MediaBrowser::~MediaBrowser()
{
	cfg.saveSettings();

	// Останавливаем загрузчик превью
	if (thumbnailLoader) {
		thumbnailLoader->cancelLoading();
	}

	if (loaderThread) {
		loaderThread->quit();
		loaderThread->wait(1000);
		delete loaderThread;
	}
}

void MediaBrowser::initPreviewArea()
{
	// Создаем область прокрутки для превью
	previewScrollArea = new QScrollArea(this);
	previewScrollArea->setWidgetResizable(true);
	previewScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	previewScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	// Создаем контейнер для миниатюр
	previewContainer = new QWidget();
	previewLayout = new QGridLayout(previewContainer);
	previewLayout->setSpacing(10);
	previewLayout->setContentsMargins(10, 10, 10, 10);

	// Устанавливаем контейнер в область прокрутки
	previewScrollArea->setWidget(previewContainer);

	// Устанавливаем область прокрутки как центральный виджет
	setCentralWidget(previewScrollArea);
	setMinimumSize(800, 600);

	// Инициализируем загрузчик превью в отдельном потоке
	thumbnailLoader = new ThumbnailLoader(cfg.ffmpegPath, cfg.thumbnailSize);
	loaderThread = new QThread();
	thumbnailLoader->moveToThread(loaderThread);

	// Подключаем сигналы от загрузчика
	connect(thumbnailLoader, &ThumbnailLoader::thumbnailLoaded,
		this, &MediaBrowser::onThumbnailLoaded);
	connect(thumbnailLoader, &ThumbnailLoader::loadingFinished,
		this, &MediaBrowser::onThumbnailsFinished);
	connect(loaderThread, &QThread::finished,
		thumbnailLoader, &QObject::deleteLater);

	// Запускаем поток загрузчика
	loaderThread->start();
}

void MediaBrowser::initSidebar()
{
	// Создаем док-виджет для боковой панели
	sidebarDock = new QDockWidget(tr("Categories"), this);
	sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	// Создаем содержимое для док-виджета
	QWidget *sidebarContent = new QWidget;
	QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarContent);

	// Создаем UI элементы
	labSrc = new QLabel(tr("Src: <NOT SELECTED>"));
	labDst = new QLabel(tr("Dst: <NOT SELECTED>"));
	moveButton = new QPushButton(tr("MOVE"));
	categoryTree = new QTreeView();
	newCategoryButton = new QPushButton(tr("New theme"));

	// Добавляем элементы в layout
	sidebarLayout->addWidget(labSrc);
	sidebarLayout->addWidget(moveButton);
	sidebarLayout->addWidget(labDst);
	sidebarLayout->addWidget(categoryTree, 1); // Растягиваем дерево
	sidebarLayout->addWidget(newCategoryButton);
	sidebarLayout->addStretch(); // Растягивающийся элемент внизу

	// Устанавливаем layout на виджет
	sidebarContent->setLayout(sidebarLayout);
	sidebarDock->setWidget(sidebarContent);

	// Добавляем док-виджет в главное окно слева
	addDockWidget(Qt::LeftDockWidgetArea, sidebarDock);

	// Настройка модели для дерева категорий
	categoriesModel = new QFileSystemModel(this);
	categoriesModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

	// Устанавливаем корневую папку из настроек
	if (cfg.targetRoot.isEmpty() || !QDir(cfg.targetRoot).exists()) {
		// Если папка не существует, используем домашнюю
		cfg.targetRoot = QDir::homePath() + "/Media_Categories";
		QDir().mkpath(cfg.targetRoot);
		cfg.saveSettings();
	}

	categoriesModel->setRootPath(cfg.targetRoot);
	categoryTree->setModel(categoriesModel);
	categoryTree->setRootIndex(categoriesModel->index(cfg.targetRoot));

	// Обновляем подписи
	updateSourceLabel();
	updateTargetLabel();

	// Настройка внешнего вида док-панели
	sidebarDock->setFeatures(QDockWidget::DockWidgetMovable |
		QDockWidget::DockWidgetFloatable |
		QDockWidget::DockWidgetClosable);

	// Настройка дерева категорий
	categoryTree->setHeaderHidden(true);
	categoryTree->setAnimated(true);
	categoryTree->hideColumn(1); // Скрываем колонки размера, типа и даты
	categoryTree->hideColumn(2);
	categoryTree->hideColumn(3);

	// Стиль выделения
	QString styleSheet =
		"QTreeView::item:selected:!active {"
		"    background-color: #2196F3;"
		"    color: #000000;"
		"}";
	categoryTree->setStyleSheet(styleSheet);

	// Подключаем сигналы кнопок (функции будут реализованы позже)
	// connect(newCategoryButton, &QPushButton::clicked, this, &MediaBrowser::createNewCategory);
	// connect(moveButton, &QPushButton::clicked, this, &MediaBrowser::moveCurrentFolder);
}

// Вспомогательные функции для обновления надписей
void MediaBrowser::updateSourceLabel()
{
	QString text = tr("Src: ");
	if (cfg.sourceRoot.isEmpty() || !QDir(cfg.sourceRoot).exists()) {
		text += "<NOT SELECTED>";
	}
	else {
		text += cfg.sourceRoot;
	}
	if (labSrc) {
		labSrc->setText(text);
	}
}

void MediaBrowser::updateTargetLabel()
{
	QString text = tr("Dst: ");
	if (cfg.targetRoot.isEmpty() || !QDir(cfg.targetRoot).exists()) {
		text += "<NOT SELECTED>";
	}
	else {
		text += cfg.targetRoot;
	}
	if (labDst) {
		labDst->setText(text);
	}
}

void MediaBrowser::initMenu()
{
	// Создаем меню File
	QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

	// Пункт: Select source folder
	QAction *selectSourceAction = fileMenu->addAction(tr("&Select source folder"));
	selectSourceAction->setShortcut(QKeySequence("Ctrl+O"));
	connect(selectSourceAction, &QAction::triggered, this, &MediaBrowser::onSelectSourceRoot);

	// Пункт: Select categories root
	QAction *selectCategoriesAction = fileMenu->addAction(tr("Select &categories root"));
	selectCategoriesAction->setShortcut(QKeySequence("Ctrl+R"));
	connect(selectCategoriesAction, &QAction::triggered, this, &MediaBrowser::onSelectTargetRoot);
		

	fileMenu->addSeparator();

	// Пункт: Move to custom folder
	QAction *moveCustomAction = fileMenu->addAction(tr("Move to &custom folder"));
	moveCustomAction->setShortcut(QKeySequence("Ctrl+M"));
	connect(moveCustomAction, &QAction::triggered, this, [this]() {
		// TODO: Реализовать перемещение в произвольную папку
		qDebug() << "Move to custom folder clicked";
	});

	// Пункт: Delete selected items
	QAction *deleteSelectedAction = fileMenu->addAction(tr("&Delete selected items"));
	deleteSelectedAction->setShortcut(QKeySequence("Del"));
	connect(deleteSelectedAction, &QAction::triggered, this, [this]() {
		// TODO: Реализовать удаление выбранных элементов
		qDebug() << "Delete selected items clicked";
	});

	// Пункт: Delete current folder
	QAction *deleteFolderAction = fileMenu->addAction(tr("Delete current &folder"));
	deleteFolderAction->setShortcut(QKeySequence("Ctrl+Del"));
	connect(deleteFolderAction, &QAction::triggered, this, [this]() {
		// TODO: Реализовать удаление текущей папки
		qDebug() << "Delete current folder clicked";
	});

	fileMenu->addSeparator();

	// Пункт: Quit
	QAction *quitAction = fileMenu->addAction(tr("&Quit"));
	quitAction->setShortcut(QKeySequence::Quit);
	connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

void MediaBrowser::onSelectSourceRoot()
{
	QString folder = QFileDialog::getExistingDirectory(
		this,
		"Select source root folder",
		cfg.sourceRoot.isEmpty() ? QDir::homePath() : cfg.sourceRoot
	);

	if (!folder.isEmpty()) {
		cfg.sourceRoot = folder;
		cfg.saveSettings();
		updateSourceLabel();
		// Сразу загружаем первую папку из нового источника
		loadNextUnprocessedFolder();
	}
}

void MediaBrowser::onSelectTargetRoot()
{
	QString folder = QFileDialog::getExistingDirectory(
		this,
		"Select target root",
		cfg.targetRoot.isEmpty() ? QDir::homePath() : cfg.targetRoot
	);

	if (!folder.isEmpty()) {
		cfg.targetRoot = folder;
		cfg.saveSettings();

		// Обновляем модель дерева категорий
		if (categoriesModel && categoryTree) {
			categoriesModel->setRootPath(cfg.targetRoot);
			categoryTree->setRootIndex(categoriesModel->index(cfg.targetRoot));
		}

		updateTargetLabel();
	}
}

void MediaBrowser::loadFolderThumbnails(const QString& folderPath)
{
	// Отменяем текущую загрузку, если есть
	if (thumbnailLoader) {
		thumbnailLoader->cancelLoading();
		QThread::msleep(100); // Даем время на отмену
	}

	// Очищаем старые превью
	clearThumbnails();

	currentFolder = folderPath;
	setWindowTitle("Media Browser - " + QFileInfo(folderPath).fileName());

	// Обновляем label с текущей папкой
	if (labSrc) {
		QString text = tr("Current: ") + QFileInfo(folderPath).fileName();
		labSrc->setText(text);
	}

	// Получаем список файлов в папке
	QDir dir(folderPath);

	QStringList imageFilters;
	imageFilters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif"
		<< "*.tiff" << "*.webp" << "*.JPG" << "*.JPEG" << "*.PNG";

	QStringList videoFilters;
	videoFilters << "*.mp4" << "*.avi" << "*.mkv" << "*.mov" << "*.wmv"
		<< "*.flv" << "*.m4v" << "*.mpg" << "*.mpeg" << "*.3gp"
		<< "*.MP4" << "*.AVI" << "*.MKV" << "*.MOV";

	QStringList allFilters = imageFilters + videoFilters;
	currentFiles = dir.entryList(allFilters, QDir::Files, QDir::Name).toVector();

	// Обновляем статус
	if (statusBar()) {
		statusBar()->showMessage(QString("Loading %1 files...").arg(currentFiles.size()));
	}

	// Создаем контейнеры для превью
	const int columns = cfg.thumbnailColumns;
	for (int i = 0; i < currentFiles.size(); ++i) {
		QLabel *thumbnailLabel = new QLabel(previewContainer);
		thumbnailLabel->setFixedSize(cfg.thumbnailSize + 20, cfg.thumbnailSize + 20);
		thumbnailLabel->setAlignment(Qt::AlignCenter);
		thumbnailLabel->setStyleSheet("border: 1px solid #ccc; background: #f0f0f0;");
		thumbnailLabel->setText("Loading...");

		int row = i / columns;
		int col = i % columns;

		previewLayout->addWidget(thumbnailLabel, row, col, Qt::AlignCenter);
		thumbnailLabels.append(thumbnailLabel);
	}

	// Активируем кнопку перемещения
	if (moveButton) {
		moveButton->setEnabled(true);
	}

	// Запускаем загрузку превью
	if (thumbnailLoader) {
		QMetaObject::invokeMethod(thumbnailLoader, "loadThumbnails",
			Qt::QueuedConnection,
			Q_ARG(QString, folderPath));
	}
}

void MediaBrowser::clearThumbnails()
{
	// Удаляем все превью
	for (QLabel *label : thumbnailLabels) {
		if (label) {
			previewLayout->removeWidget(label);
			delete label;
		}
	}
	thumbnailLabels.clear();
	currentFiles.clear();
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
	statusBar()->showMessage("Loading finished", 3000);
}

bool MediaBrowser::eventFilter(QObject *obj, QEvent *event)
{
	// Реализация открытия файла
	if (event->type() == QEvent::MouseButtonDblClick) {
		QLabel *label = qobject_cast<QLabel*>(obj);
		if (label) {
			int index = label->property("fileIndex").toInt();
			if (index >= 0 && index < currentFiles.size()) {
				QString filePath = cfg.sourceRoot + "/" + currentFiles[index];
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
	if (cfg.sourceRoot.isEmpty()) return QString();

	// Ищем первую попавшуюся папку в корне-источнике
	QDirIterator it(cfg.sourceRoot,
		QStringList(),  // Пустой фильтр - любые имена
		QDir::Dirs | QDir::NoDotAndDotDot,
		QDirIterator::NoIteratorFlags);

	if (it.hasNext()) {
		return it.next();
	}

	return QString(); // папок не найдено
}

void MediaBrowser::loadNextUnprocessedFolder()
{
	// Находим следующую необработанную папку
	QString nextFolder = findNextUnprocessedDir();

	if (nextFolder.isEmpty()) {
		// Нет папок для обработки
		qDebug() << "No folders to process in source root:" << cfg.sourceRoot;

		// Обновляем UI
		if (labSrc) {
			labSrc->setText(tr("No more folders to process"));
		}
		if (moveButton) {
			moveButton->setEnabled(false);
		}
		if (statusBar()) {
			statusBar()->showMessage(tr("All folders processed"), 3000);
		}
		return;
	}

	// Загружаем превью для найденной папки
	loadFolderThumbnails(nextFolder);
}

void MediaBrowser::moveCurrentFolder()
{
	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Ошибка", "Нет текущей папки для перемещения");
		return;
	}

	if (cfg.targetRoot.isEmpty() || !QDir(cfg.targetRoot).exists()) {
		QMessageBox::warning(this, "Ошибка", "Целевая папка не выбрана или не существует");
		return;
	}

	// Получаем имя папки
	QString folderName = QFileInfo(currentFolder).fileName();
	QString targetPath = QDir(cfg.targetRoot).absoluteFilePath(folderName);

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
	QDir sourceDir(cfg.sourceRoot);
	QStringList subdirs = sourceDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

	if (!subdirs.isEmpty()) {
		QString nextFolder = QDir(cfg.sourceRoot).absoluteFilePath(subdirs.first());
		loadFolderThumbnails(nextFolder);
	}
	else {
		currentFolder.clear();
		QMessageBox::information(this, "Завершено",
			"Все папки в исходной директории обработаны.");
	}
}
