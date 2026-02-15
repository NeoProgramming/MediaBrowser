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
	, categoriesPanel(nullptr)
	, tagsPanel(nullptr)
	, previewArea(nullptr)
	, thumbnailLoader(nullptr)
	, loaderThread(nullptr)
{
	// Загружаем настройки
	cfg.loadSettings();

	// Инициализируем менеджер тегов
	tagManager = new TagManager(this);
	if (!cfg.tagsPath.isEmpty()) {
		tagManager->loadTags(cfg.tagsPath);
	}

	// Инициализируем UI компоненты
	initPreviewArea();
	initSidebar();
	initTagsbar();
	initMenu();
	initGeometry();
	
	// Загружаем первую папку через таймер (после инициализации UI)
	QTimer::singleShot(100, this, &MediaBrowser::loadNextUnprocessedFolder);
}

MediaBrowser::~MediaBrowser()
{
	// Сохраняем геометрию перед закрытием
	cfg.windowGeometry = saveGeometry();
	cfg.windowState = saveState();

	// Сохраняем ширины панелей
	if (categoriesPanel) {
		cfg.leftPanelWidth = categoriesPanel->width();
	}
	if (tagsPanel) {
		cfg.rightPanelWidth = tagsPanel->width();
	}

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

	// Устанавливаем как центральный виджет
	setCentralWidget(previewArea);
	
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
	// Создаем панель категорий
	categoriesPanel = new CategoriesPanel(this);
	categoriesPanel->setTargetRoot(cfg.targetRoot);

	addDockWidget(Qt::LeftDockWidgetArea, categoriesPanel);

	// Подключаем сигналы от панели категорий
	connect(categoriesPanel, &CategoriesPanel::moveSelectedRequested,
		this, &MediaBrowser::onMoveSelectedClicked);
	connect(categoriesPanel, &CategoriesPanel::moveAllRequested,
		this, &MediaBrowser::onMoveAllClicked);
}

void MediaBrowser::initTagsbar()
{
	// Создаем панель тегов (справа)
	tagsPanel = new TagsPanel(this);

	addDockWidget(Qt::RightDockWidgetArea, tagsPanel);

	// Подключаем сигналы от панели тегов
	connect(tagsPanel, &TagsPanel::tagToggled,
		this, &MediaBrowser::onTagToggled);
	connect(tagsPanel, &TagsPanel::addTagClicked,
		this, &MediaBrowser::onAddTagClicked);
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

	// Пункт: Move selected items
	QAction *moveSelectedAction = fileMenu->addAction(tr("Move selected items to custom folder"));
	connect(moveSelectedAction, &QAction::triggered,
		this, &MediaBrowser::onMoveSelectedToCustomFolder);

	// Пункт: Move selected items
	QAction *moveFolderAction = fileMenu->addAction(tr("Move current folder to custom folder"));
	connect(moveFolderAction, &QAction::triggered,
		this, &MediaBrowser::onMoveFolderToCustomFolder);
	
	// Пункт: Delete selected items
	QAction *deleteSelectedAction = fileMenu->addAction(tr("Delete selected items"));
	connect(deleteSelectedAction, &QAction::triggered,
		this, &MediaBrowser::onDeleteSelectedItems);

	// Пункт: Delete current folder
	QAction *deleteFolderAction = fileMenu->addAction(tr("Delete current folder"));
	connect(deleteFolderAction, &QAction::triggered,
		this, &MediaBrowser::onDeleteCurrentFolder);

	fileMenu->addSeparator();

	// Пункт: Quit
	QAction *quitAction = fileMenu->addAction(tr("&Quit"));
	quitAction->setShortcut(QKeySequence::Quit);
	connect(quitAction, &QAction::triggered, this, &QWidget::close);


	QMenu *viewMenu = menuBar()->addMenu("&View");

	QAction *showCategoriesAction = viewMenu->addAction("Show &categories panel");
	showCategoriesAction->setCheckable(true);
	showCategoriesAction->setChecked(true);
	connect(showCategoriesAction, &QAction::toggled,
		categoriesPanel, &QDockWidget::setVisible);


	QMenu *helpMenu = menuBar()->addMenu("&Help");

	QAction *aboutAction = helpMenu->addAction("&About");
	connect(aboutAction, &QAction::triggered, this, [this]() {
		QMessageBox::about(this, "Media Browser",
			"Media sorting tool\n"
			"Version 1.0\n\n"
			"Left panel: Target categories\n"
			"Center: Current folder previews\n"
			"Click MOVE to move current folder to selected category");
	});
}

void MediaBrowser::initGeometry()
{
	// Восстанавливаем геометрию окна
	if (!cfg.windowGeometry.isEmpty()) {
		restoreGeometry(cfg.windowGeometry);
	}
	else {
		// Если нет сохраненной геометрии, устанавливаем размер по умолчанию
		resize(1200, 800);
	}

	if (!cfg.windowState.isEmpty()) {
		restoreState(cfg.windowState);
	}

	// Устанавливаем сохраненные ширины панелей
	if (categoriesPanel && cfg.leftPanelWidth > 0) {
		categoriesPanel->resize(cfg.leftPanelWidth, categoriesPanel->height());
	}

	if (tagsPanel && cfg.rightPanelWidth > 0) {
		tagsPanel->resize(cfg.rightPanelWidth, tagsPanel->height());
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
	previewArea->setTotalCount(currentFiles.size());

	// Устанавливаем имена файлов в плейсхолдеры
	for (int i = 0; i < currentFiles.size(); ++i) {
		previewArea->setFilename(i, currentFiles[i]);
	}

	// Обновляем статус-бар с полной информацией
	statusLoading = QString("Loading %1 files...").arg(currentFiles.size());
	updateStatusBar();

	// Запускаем загрузку превью
	if (thumbnailLoader) {
		QMetaObject::invokeMethod(thumbnailLoader, "loadThumbnails",
			Qt::QueuedConnection,
			Q_ARG(QString, folderPath));
	}
}


void MediaBrowser::onSelectionChanged(const QSet<int>& selectedIndices)
{
	// Обновляем статус или передаем в панель тегов
	selectedFileIndices = selectedIndices;
		
	updateStatusBar();
	updateTagsPanel();
}

void MediaBrowser::onSelectionCleared()
{
	selectedFileIndices.clear();

	statusBar()->showMessage("Selection cleared", 2000);

	updateTagsPanel();
}

void MediaBrowser::updateTagsPanel()
{
	if (selectedFileIndices.isEmpty()) {
		// Если ничего не выбрано - показываем теги текущей папки
		QSet<QString> folderTags = tagManager->getObjectTags(currentFolder);
		tagsPanel->setObjectTags(folderTags);
		tagsPanel->setObjectName(QFileInfo(currentFolder).fileName());
	}
	else if (selectedFileIndices.size() == 1) {
		// Если выбран один файл
		int index = *selectedFileIndices.begin();
		if (index < currentFiles.size()) {
			QString filePath = QDir(currentFolder).absoluteFilePath(currentFiles[index]);
			QSet<QString> fileTags = tagManager->getObjectTags(filePath);
			tagsPanel->setObjectTags(fileTags);
			tagsPanel->setObjectName(currentFiles[index]);
		}
	}
	else {
		// Если выбрано несколько файлов - объединение тегов
		QSet<QString> allFileTags;
		for (int index : selectedFileIndices) {
			if (index < currentFiles.size()) {
				QString filePath = QDir(currentFolder).absoluteFilePath(currentFiles[index]);
				allFileTags.unite(tagManager->getObjectTags(filePath));
			}
		}
		tagsPanel->setObjectTags(allFileTags);
		tagsPanel->setObjectName(QString("%1 files").arg(selectedFileIndices.size()));
	}

	// Устанавливаем все доступные теги
	tagsPanel->setAllTags(tagManager->getAllTags());
}

void MediaBrowser::onThumbnailsFinished()
{
	statusLoading = "Loading finished";
	updateStatusBar();
}

void MediaBrowser::onThumbnailLoaderError(const QString& error)
{
	qDebug() << "ThumbnailLoader error:" << error;
	statusLoading = "Error: " + error;
	updateStatusBar();
}

// Слоты для PreviewArea

void MediaBrowser::onThumbnailClicked(int index, Qt::KeyboardModifiers modifiers)
{
	qDebug() << "MediaBrowser::onThumbnailClicked - index:" << index;
	qDebug() << "Current files count:" << currentFiles.size();

	if (index >= 0 && index < currentFiles.size()) {
		qDebug() << "  File at this index:" << currentFiles[index];
	}
	else {
		qDebug() << "  WARNING: Index out of range!";
	}
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
		if (previewArea && previewArea->getTotalCount() > 0) {
			QSet<int> allIndices;
			for (int i = 0; i < previewArea->getTotalCount(); ++i) {
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
				
		if (statusBar()) {
			statusBar()->showMessage(tr("All folders processed"), 3000);
		}
		return;
	}

	// Загружаем превью для найденной папки
	loadFolderThumbnails(nextFolder);

	// Загружаем теги
	selectedFileIndices.clear();
	updateTagsPanel();
}



// Переименовываем старый слот и добавляем новый
void MediaBrowser::onMoveSelectedClicked(const QString& targetCategory)
{
	if (selectedFileIndices.isEmpty()) {
		QMessageBox::warning(this, "Error", "No files selected");
		return;
	}

	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Error", "No current folder");
		return;
	}

	// Перемещаем только выбранные файлы
	moveSelectedFiles(targetCategory);
}

void MediaBrowser::onMoveAllClicked(const QString& targetCategory)
{
	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Error", "No current folder to move");
		return;
	}

	// Перемещаем всю папку
	moveCurrentFolder(targetCategory);
}

// Новая функция для перемещения выбранных файлов
void MediaBrowser::moveSelectedFiles(const QString& targetCategory)
{
	qDebug() << "Moving selected files to:" << targetCategory;

	// Получаем имена выбранных файлов
	QStringList selectedFiles;
	for (int index : selectedFileIndices) {
		if (index < currentFiles.size()) {
			selectedFiles.append(currentFiles[index]);
		}
	}

	if (selectedFiles.isEmpty()) {
		QMessageBox::warning(this, "Error", "No valid files selected");
		return;
	}

	// Перемещаем каждый файл
	int movedCount = 0;
	QDir sourceDir(currentFolder);
	QDir targetDir(targetCategory);

	for (const QString &filename : selectedFiles) {
		QString sourcePath = sourceDir.absoluteFilePath(filename);
		QString targetPath = targetDir.absoluteFilePath(filename);

		// Проверяем, существует ли уже файл в целевой папке
		if (QFile::exists(targetPath)) {
			int result = QMessageBox::question(this, "Confirm Overwrite",
				QString("File '%1' already exists in target folder.\nOverwrite?").arg(filename),
				QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

			if (result == QMessageBox::No) {
				continue; // Пропускаем этот файл
			}
			else if (result == QMessageBox::Cancel) {
				break; // Отменяем операцию
			}
		}

		// Перемещаем файл
		if (QFile::rename(sourcePath, targetPath)) {
			movedCount++;

			// Также перемещаем .tags файл если он существует
			QString tagsSourcePath = sourcePath + ".tags";
			QString tagsTargetPath = targetPath + ".tags";
			if (QFile::exists(tagsSourcePath)) {
				QFile::rename(tagsSourcePath, tagsTargetPath);
			}

			// Удаляем из текущего списка файлов
			currentFiles.removeAll(filename);
		}
		else {
			QMessageBox::warning(this, "Error",
				QString("Failed to move file:\n%1").arg(filename));
		}
	}

	// Обновляем отображение
	if (movedCount > 0) {
		// Получаем индексы перемещенных файлов
		QList<int> movedIndices = selectedFileIndices.values();//@ .toList();

		// Удаляем из currentFiles
		QStringList movedFilenames;
		for (int index : movedIndices) {
			if (index < currentFiles.size()) {
				movedFilenames.append(currentFiles[index]);
			}
		}

		for (const QString& filename : movedFilenames) {
			currentFiles.removeAll(filename);
		}

		// ВАЖНО: Не перезагружаем всё, а просто удаляем из PreviewArea
		previewArea->removeFiles(movedIndices);

		// Очищаем выделение
		selectedFileIndices.clear();
		previewArea->clearSelection();

		// Обновляем статус-бар
		updateStatusBar();

		// Обновляем теги
		updateTagsPanel();

		statusBar()->showMessage(QString("Moved %1 files to %2").arg(movedCount).arg(targetCategory), 5000);

		//reloadCurrentFolder();
	}
}

void MediaBrowser::reloadCurrentFolder()
{
	// ВАЖНО: Не просто обновляем количество превью, а перезагружаем всю папку
		// Сначала очищаем выбранные файлы
	selectedFileIndices.clear();

	// Затем перезагружаем текущую папку, чтобы обновить список файлов
	if (!currentFolder.isEmpty()) {
		loadFolderThumbnails(currentFolder);
	}

	// Обновляем теги
	updateTagsPanel();
}

// Обновляем старую функцию moveCurrentFolder
void MediaBrowser::moveCurrentFolder(const QString& targetCategory)
{
	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Error", "No current folder to move");
		return;
	}

	// Проверяем, что целевая папка существует
	QDir targetDir(targetCategory);
	if (!targetDir.exists()) {
		QMessageBox::warning(this, "Error",
			QString("Target folder does not exist:\n%1").arg(targetCategory));
		return;
	}

	// Получаем имя папки
	QString folderName = QFileInfo(currentFolder).fileName();
	QString targetPath = QDir(targetCategory).absoluteFilePath(folderName);

	// Проверяем, не существует ли уже такая папка
	if (QDir(targetPath).exists()) {
		int result = QMessageBox::question(this, "Confirm Overwrite",
			QString("Folder '%1' already exists in target location.\nOverwrite?").arg(folderName),
			QMessageBox::Yes | QMessageBox::No);

		if (result == QMessageBox::No) {
			return;
		}

		// Если перезаписываем, удаляем старую папку
		//@ todo заменить!
		if (!QDir(targetPath).removeRecursively()) {
			QMessageBox::warning(this, "Error",
				QString("Failed to remove existing folder:\n%1").arg(targetPath));
			return;
		}
	}

	// Перемещаем папку
	QDir dir;
	if (dir.rename(currentFolder, targetPath)) {
		statusBar()->showMessage(QString("Folder moved to: %1").arg(targetPath), 5000);

		// Перемещаем теги папки если они существуют
		QString tagsSourcePath = currentFolder + ".tags";
		QString tagsTargetPath = targetPath + ".tags";
		if (QFile::exists(tagsSourcePath)) {
			QFile::rename(tagsSourcePath, tagsTargetPath);
		}

		// Загружаем следующую папку
		loadNextUnprocessedFolder();
	}
	else {
		QMessageBox::warning(this, "Error",
			QString("Failed to move folder.\nFrom: %1\nTo: %2").arg(currentFolder).arg(targetPath));
	}
}

// Выбор исходного корня (через меню)
void MediaBrowser::onSelectSourceRoot()
{
	QString folder = QFileDialog::getExistingDirectory(
		this,
		"Select Source Root Folder",
		cfg.sourceRoot.isEmpty() ? QDir::homePath() : cfg.sourceRoot
	);

	if (!folder.isEmpty()) {
		cfg.sourceRoot = folder;
		cfg.saveSettings();
		loadNextUnprocessedFolder(); // Начинаем обработку с новой папки
	}
}

// Выбор целевого корня 
void MediaBrowser::onSelectTargetRoot()
{
	// MediaBrowser сам обрабатывает выбор папки
	QString folder = QFileDialog::getExistingDirectory(
		this,
		"Select Target Root Folder",
		cfg.targetRoot.isEmpty() ? QDir::homePath() : cfg.targetRoot
	);

	if (!folder.isEmpty()) {
		cfg.targetRoot = folder;
		cfg.saveSettings();
		categoriesPanel->setTargetRoot(folder);  // Передаем в панель
	}
}

// Обработка изменения тега
void MediaBrowser::onTagToggled(const QString& tag, bool checked)
{
	Q_UNUSED(tag);
	Q_UNUSED(checked);

	// Для текущего контекста (файл, папка или несколько файлов)
	// применяем изменение тега
	updateObjectTags(tagsPanel->getSelectedTags());
}

// Добавление нового тега
void MediaBrowser::onAddTagClicked(const QString& newTag)
{
	if (newTag.isEmpty()) return;

	// Добавляем тег в общий список
	tagManager->addGlobalTag(newTag);

	// Получаем текущие теги
	QSet<QString> currentTags = tagsPanel->getSelectedTags();
	currentTags.insert(newTag);

	// Обновляем
	updateObjectTags(currentTags);

	// Очищаем поле ввода
	tagsPanel->clearInput();
}

// Обновление тегов объекта
void MediaBrowser::updateObjectTags(const QSet<QString>& newTags)
{
	if (selectedFileIndices.isEmpty()) {
		// Для текущей папки
		if (!currentFolder.isEmpty()) {
			tagManager->setObjectTags(currentFolder, newTags);
		}
	}
	else {
		// Для выбранных файлов
		for (int index : selectedFileIndices) {
			if (index < currentFiles.size()) {
				QString filePath = QDir(currentFolder).absoluteFilePath(currentFiles[index]);
				tagManager->setObjectTags(filePath, newTags);
			}
		}
	}

	// Обновляем список всех тегов
	tagsPanel->setAllTags(tagManager->getAllTags());
}

void MediaBrowser::onMoveSelectedToCustomFolder()
{
	qDebug() << "Move selected items to custom folder";

	// Проверяем, есть ли выбранные файлы
	if (selectedFileIndices.isEmpty()) {
		QMessageBox::warning(this, "Error",
			"No files selected.\nPlease select files to move.");
		return;
	}

	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Error", "No current folder");
		return;
	}

	// Диалог выбора папки
	QString folder = QFileDialog::getExistingDirectory(
		this,
		"Select Destination Folder",
		cfg.targetRoot.isEmpty() ? QDir::homePath() : cfg.targetRoot,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (!folder.isEmpty()) {
		// Перемещаем выбранные файлы
		moveSelectedFiles(folder);
	}
}

void MediaBrowser::onMoveFolderToCustomFolder()
{
	qDebug() << "Move current folder to custom folder";

	// Проверяем, есть ли текущая папка
	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Error",
			"No current folder.\nPlease load a folder first.");
		return;
	}

	// Диалог выбора папки
	QString folder = QFileDialog::getExistingDirectory(
		this,
		"Select Destination Folder",
		cfg.targetRoot.isEmpty() ? QDir::homePath() : cfg.targetRoot,
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
	);

	if (!folder.isEmpty()) {
		// Перемещаем всю папку
		moveCurrentFolder(folder);
	}
}

void MediaBrowser::onDeleteSelectedItems()
{
	qDebug() << "Delete selected items";

	// Проверяем, есть ли выбранные файлы
	if (selectedFileIndices.isEmpty()) {
		QMessageBox::warning(this, "Error",
			"No files selected.\nPlease select files to delete.");
		return;
	}

	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Error", "No current folder");
		return;
	}

	// Получаем имена выбранных файлов
	QStringList selectedFiles;
	for (int index : selectedFileIndices) {
		if (index < currentFiles.size()) {
			selectedFiles.append(currentFiles[index]);
		}
	}

	if (selectedFiles.isEmpty()) {
		QMessageBox::warning(this, "Error", "No valid files selected");
		return;
	}

	// Запрос подтверждения
	QString message;
	if (selectedFiles.size() == 1) {
		message = QString("Are you sure you want to permanently delete the file:\n\n%1")
			.arg(selectedFiles.first());
	}
	else {
		message = QString("Are you sure you want to permanently delete %1 selected files?")
			.arg(selectedFiles.size());
	}

	QMessageBox::StandardButton reply = QMessageBox::question(
		this,
		"Confirm Delete",
		message,
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No
	);

	if (reply == QMessageBox::Yes) {
		// Удаляем файлы
		deleteFiles(selectedFiles);
	}
}

void MediaBrowser::onDeleteCurrentFolder()
{
	qDebug() << "Delete current folder";

	// Проверяем, есть ли текущая папка
	if (currentFolder.isEmpty()) {
		QMessageBox::warning(this, "Error",
			"No current folder.\nPlease load a folder first.");
		return;
	}

	QFileInfo folderInfo(currentFolder);
	QString folderName = folderInfo.fileName();

	// Получаем количество файлов в папке
	QDir dir(currentFolder);
	QStringList allFiles = dir.entryList(QDir::Files);
	int fileCount = allFiles.size();

	// Запрос подтверждения (более строгий для удаления папки)
	QString message = QString(
		"Are you sure you want to permanently delete the folder:\n\n"
		"%1\n\n"
		"This folder contains %2 file(s).\n"
		"This operation CANNOT be undone!"
	).arg(currentFolder).arg(fileCount);

	QMessageBox::StandardButton reply = QMessageBox::question(
		this,
		"Confirm Delete Folder",
		message,
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No
	);

	if (reply == QMessageBox::Yes) {
		// Дополнительное подтверждение для папок с файлами
		if (fileCount > 0) {
			QMessageBox::StandardButton secondReply = QMessageBox::question(
				this,
				"Final Confirmation",
				QString("You are about to delete %1 file(s).\n"
					"Are you ABSOLUTELY sure?").arg(fileCount),
				QMessageBox::Yes | QMessageBox::No,
				QMessageBox::No
			);

			if (secondReply != QMessageBox::Yes) {
				return;
			}
		}

		// Удаляем папку
		deleteFolder(currentFolder);
	}
}

void MediaBrowser::deleteFiles(const QStringList& filenames)
{
	qDebug() << "Deleting files:" << filenames;

	if (filenames.isEmpty()) return;

	// Находим индексы удаляемых файлов
	QList<int> indicesToDelete;
	for (const QString& filename : filenames) {
		int index = currentFiles.indexOf(filename);
		if (index >= 0) {
			indicesToDelete.append(index);
		}
	}

	int deletedCount = 0;
	int failedCount = 0;
	QDir sourceDir(currentFolder);

	for (const QString &filename : filenames) {
		QString filePath = sourceDir.absoluteFilePath(filename);

		// Удаляем файл
		if (QFile::remove(filePath)) {
			deletedCount++;

			// Удаляем .tags файл если он существует
			QString tagsPath = filePath + ".tags";
			if (QFile::exists(tagsPath)) {
				QFile::remove(tagsPath);
			}

			// Удаляем из текущего списка файлов
			currentFiles.removeAll(filename);

			qDebug() << "Deleted:" << filename;
		}
		else {
			failedCount++;
			qDebug() << "Failed to delete:" << filename;
		}
	}

	// Обновляем отображение
	if (deletedCount > 0) {

		// Удаляем из currentFiles
		for (const QString& filename : filenames) {
			currentFiles.removeAll(filename);
		}

		// ВАЖНО: Точечное удаление из PreviewArea
		previewArea->removeFiles(indicesToDelete);

		// Очищаем выделение
		selectedFileIndices.clear();
		previewArea->clearSelection();

		// Обновляем статус-бар
		updateStatusBar();

		// Обновляем теги
		updateTagsPanel();

		statusBar()->showMessage(
			QString("Deleted %1 files, %2 failed").arg(deletedCount).arg(failedCount),
			5000
		);

		// Очищаем выделение и перезагружаем текущую папку
	//	selectedFileIndices.clear();
	//	previewArea->clearSelection();
	//	reloadCurrentFolder();
	//	updateTagsPanel();
	}

	if (failedCount > 0) {
		QMessageBox::warning(this, "Delete Failed",
			QString("Failed to delete %1 file(s).\n"
				"They may be in use or you don't have permission.")
			.arg(failedCount));
	}
}

void MediaBrowser::deleteFolder(const QString& folderPath)
{
	qDebug() << "Deleting folder:" << folderPath;

	QDir dir(folderPath);
	QString folderName = dir.dirName();

	// Рекурсивно удаляем все содержимое
	bool success = dir.removeRecursively();

	if (success) {
		statusBar()->showMessage(QString("Deleted folder: %1").arg(folderPath), 5000);

		// Загружаем следующую папку
		loadNextUnprocessedFolder();
	}
	else {
		QMessageBox::warning(this, "Delete Failed",
			QString("Failed to delete folder:\n%1\n\n"
				"It may be in use, not empty, or you don't have permission.")
			.arg(folderPath));
	}
}

void MediaBrowser::updateStatusBar()
{
	if (!statusBar()) return;

	QString statusText;

	// 1. Информация о текущей папке
	if (!currentFolder.isEmpty()) {
		
		// Получаем статистику из кэша
		int totalFiles = getTotalFilesCount(currentFolder);
		int totalFolders = getTotalFoldersCount(currentFolder);
		int mediaFiles = currentFiles.size();
		int otherFiles = totalFiles - mediaFiles;

		// Формируем строку статистики папки
		statusText += QString(" %1 files").arg(totalFiles);
		statusText += QString(", %1 subfolders").arg(totalFolders);
		statusText += QString(" | %1 media").arg(mediaFiles);

		if (otherFiles > 0) {
			statusText += QString(", %1 other").arg(otherFiles);
		}
	}

	// 2. Информация о выделении
	if (!selectedFileIndices.isEmpty()) {
		statusText += QString(" | Selected: %1 files").arg(selectedFileIndices.size());
	}
	else {
		if (!currentFolder.isEmpty()) {
			statusText += QString(" | No selection");
		}
	}
	
	if (!statusLoading.isEmpty()) {
		statusText += " | ";
		statusText += statusLoading;
	}

	statusBar()->showMessage(statusText);
}

// Новые методы для подсчета статистики
int MediaBrowser::getTotalFilesCount(const QString& folderPath)
{
	QDir dir(folderPath);
	dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
	return dir.entryList().count();
}

int MediaBrowser::getTotalFoldersCount(const QString& folderPath)
{
	QDir dir(folderPath);
	dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
	return dir.entryList().count();
}
