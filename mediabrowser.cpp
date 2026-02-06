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
	, previewArea(nullptr)
	, thumbnailLoader(nullptr)
	, loaderThread(nullptr)
{
	// Загружаем настройки
	cfg.loadSettings();

	// Инициализируем UI компоненты
	initPreviewArea();
	initSidebar();
	initMenu();

	resize(1200, 800);

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
	// Создаем область превью
	previewArea = new PreviewArea(this);
	previewArea->setThumbnailSize(cfg.thumbnailSize);
	previewArea->setColumns(cfg.thumbnailColumns);

	// Устанавливаем как центральный виджет
	setCentralWidget(previewArea);
	/*
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

	*/

	// Инициализируем загрузчик превью в отдельном потоке
	thumbnailLoader = new ThumbnailLoader(cfg.ffmpegPath, cfg.thumbnailSize);
	loaderThread = new QThread();
	thumbnailLoader->moveToThread(loaderThread);


	// Подключаем сигналы загрузчика к PreviewArea
	connect(thumbnailLoader, &ThumbnailLoader::thumbnailLoaded,
		previewArea, &PreviewArea::onThumbnailLoaded);
	connect(thumbnailLoader, &ThumbnailLoader::loadingFinished,
		this, &MediaBrowser::onThumbnailsFinished);
	connect(thumbnailLoader, &ThumbnailLoader::errorOccurred,
		this, &MediaBrowser::onThumbnailLoaderError);
	connect(loaderThread, &QThread::finished,
		thumbnailLoader, &QObject::deleteLater);

	// Подключаем сигналы от PreviewArea
	connect(previewArea, &PreviewArea::thumbnailClicked,
		this, &MediaBrowser::onThumbnailClicked);
	connect(previewArea, &PreviewArea::thumbnailDoubleClicked,
		this, &MediaBrowser::onThumbnailDoubleClicked);
	connect(previewArea, &PreviewArea::selectionChanged,
		this, &MediaBrowser::onSelectionChanged);
	connect(previewArea, &PreviewArea::selectionCleared,
		this, &MediaBrowser::onSelectionCleared);

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


	currentFolder = folderPath;
	setWindowTitle("Media Browser - " + folderPath);
		
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

	// Очищаем и настраиваем PreviewArea
	previewArea->clearThumbnails();
	previewArea->setThumbnailCount(currentFiles.size());

	// Устанавливаем имена файлов в плейсхолдеры
	for (int i = 0; i < currentFiles.size(); ++i) {
		previewArea->setPlaceholder(i, "Loading...");
	}

	// Обновляем статус
	if (statusBar()) {
		statusBar()->showMessage(QString("Loading %1 files...").arg(currentFiles.size()));
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


void MediaBrowser::onSelectionChanged(const QSet<int>& selectedIndices)
{
	// Обновляем статус или передаем в будущую панель тегов
	QString message;
	if (selectedIndices.isEmpty()) {
		message = "No selection";
	}
	else if (selectedIndices.size() == 1) {
		int index = *selectedIndices.begin();
		if (index < currentFiles.size()) {
			message = QString("Selected: %1").arg(currentFiles[index]);
		}
	}
	else {
		message = QString("%1 items selected").arg(selectedIndices.size());
	}

	statusBar()->showMessage(message);

	// TODO: Когда появится TagsPanel
	// if (tagsPanel) {
	//     QStringList selectedFiles;
	//     for (int index : selectedIndices) {
	//         if (index < currentFiles.size()) {
	//             selectedFiles.append(QDir(currentFolder).absoluteFilePath(currentFiles[index]));
	//         }
	//     }
	//     tagsPanel->onFileSelectionChanged(selectedIndices, selectedFiles);
	// }
}

void MediaBrowser::onSelectionCleared()
{
	statusBar()->showMessage("Selection cleared", 2000);

	// TODO: Когда появится TagsPanel
	// if (tagsPanel) {
	//     tagsPanel->onFileSelectionChanged(QSet<int>(), QStringList());
	// }
}

void MediaBrowser::onThumbnailsFinished()
{
	statusBar()->showMessage("Loading finished", 3000);
}

void MediaBrowser::onThumbnailLoaderError(const QString& error)
{
	qDebug() << "ThumbnailLoader error:" << error;
	statusBar()->showMessage("Error: " + error, 5000);
}

// Слоты для PreviewArea

void MediaBrowser::onThumbnailClicked(int index, Qt::KeyboardModifiers modifiers)
{
	qDebug() << "Thumbnail clicked:" << index << "modifiers:" << modifiers;
	// Здесь можно добавить дополнительную логику при клике
}

void MediaBrowser::onThumbnailDoubleClicked(int index)
{
	openFile(index);
}

void MediaBrowser::openFile(int index)
{
	if (index < 0 || index >= currentFiles.size()) {
		qDebug() << "Invalid index for file open:" << index;
		return;
	}

	QString filePath = QDir(currentFolder).absoluteFilePath(currentFiles[index]);
	QFileInfo fileInfo(filePath);

	if (!fileInfo.exists()) {
		qDebug() << "File doesn't exist:" << filePath;
		QMessageBox::warning(this, "Error",
			tr("File not found:\n%1").arg(filePath));
		return;
	}

	qDebug() << "Opening file:" << filePath;

	// Открываем файл стандартной программой
	bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));

	if (!success) {
		qDebug() << "Failed to open file:" << filePath;
		QMessageBox::warning(this, "Error",
			tr("Cannot open file:\n%1").arg(filePath));
	}
}

void MediaBrowser::closeEvent(QCloseEvent *event)
{
	// Отменяем загрузку при закрытии окна
	if (thumbnailLoader) {
		thumbnailLoader->cancelLoading();
	}

	QMainWindow::closeEvent(event);
}

void MediaBrowser::keyPressEvent(QKeyEvent *event)
{
	// Escape - снять выделение
	if (event->key() == Qt::Key_Escape) {
		if (previewArea) {
			previewArea->clearSelection();
		}
		event->accept();
		return;
	}

	// Ctrl+A - выделить все
	if (event->key() == Qt::Key_A && event->modifiers() == Qt::ControlModifier) {
		if (previewArea && previewArea->getThumbnailCount() > 0) {
			QSet<int> allIndices;
			for (int i = 0; i < previewArea->getThumbnailCount(); ++i) {
				allIndices.insert(i);
			}
			previewArea->setSelection(allIndices);
		}
		event->accept();
		return;
	}
	else {
		QMainWindow::keyPressEvent(event);
	}
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
	previewArea->clearThumbnails();
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
