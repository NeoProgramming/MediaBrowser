#pragma once

#include <QMainWindow>
#include <QGridLayout>
#include <QScrollArea>
#include <QFileSystemModel>
#include <QListView>
#include <QImage>
#include <QPixmap>
#include <QDir>
#include <QStandardItemModel>
#include <QThread>
#include <QMutex>
#include <QFuture>
#include <QtConcurrent>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>
#include "Settings.h"
#include "ThumbnailWidget.h"
#include "previewarea.h"

class ThumbnailLoader;

class MediaBrowser : public QMainWindow
{
    Q_OBJECT

public:
    MediaBrowser(QWidget *parent = Q_NULLPTR);
	virtual ~MediaBrowser();
protected:
	virtual void closeEvent(QCloseEvent *event) override;
	virtual void keyPressEvent(QKeyEvent *event) override;
private slots:
	
	void onSelectSourceRoot();
	void onSelectTargetRoot();	

	// Слоты загрузчика

	void onThumbnailsFinished();
	void onThumbnailLoaderError(const QString& error);
	void onThumbnailClicked(int index, Qt::KeyboardModifiers modifiers);
	void onThumbnailDoubleClicked(int index);
	void onSelectionChanged(const QSet<int>& selectedIndices);
	void onSelectionCleared();
private:
	void initPreviewArea();
	void initSidebar();
	void initMenu();

	void moveCurrentFolder();
	QString findNextUnprocessedDir();
	void loadNextUnprocessedFolder();

	void loadFolderThumbnails(const QString& folderPath);

	void updateSourceLabel();
	void updateTargetLabel();

	void openFile(int index);

	// Настройки
	Settings cfg;

	// UI элементы боковой панели (докинг)
	QLabel *labSrc;                 // Папка-источник сортируемого контента
	QLabel *labDst;                 // Корневая папка для дерева категорий
	QDockWidget *sidebarDock;       // Докинг-панель
	QTreeView *categoryTree;        // Визуальное дерево папок-категорий
	QFileSystemModel *categoriesModel; // Модель файловой системы для дерева категорий
	QPushButton *moveButton;        // Кнопка "MOVE"
	QPushButton *newCategoryButton; // Кнопка "New theme"

	// UI элементы основной области (превью)
	PreviewArea *previewArea;           // Заменяем три переменные одной!

	// Данные текущей папки
	QString currentFolder;          // Текущая просматриваемая папка
	QVector<QString> currentFiles;  // Файлы в текущей папке
	
	 // Загрузчик превью
	ThumbnailLoader *thumbnailLoader;
	QThread *loaderThread;
};
