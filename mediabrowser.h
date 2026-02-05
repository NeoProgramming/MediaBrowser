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

class ThumbnailLoader;

class MediaBrowser : public QMainWindow
{
    Q_OBJECT

public:
    MediaBrowser(QWidget *parent = Q_NULLPTR);
	virtual ~MediaBrowser();
protected:
	virtual bool eventFilter(QObject *obj, QEvent *event) override;
	virtual void closeEvent(QCloseEvent *event) override;
private slots:
	
	void onSelectSourceRoot();
	void onSelectTargetRoot();	
	void onThumbnailLoaded(int index, const QPixmap& pixmap);
	void onThumbnailsFinished();

private:
	void initPreviewArea();
	void initSidebar();
	void initMenu();

	void clearThumbnails();
	void moveCurrentFolder();
	QString findNextUnprocessedDir();
	void loadNextUnprocessedFolder();

	void loadFolderThumbnails(const QString& folderPath);

	void updateSourceLabel();
	void updateTargetLabel();

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
	QScrollArea *previewScrollArea; // Область с прокруткой для превью
	QWidget *previewContainer;      // Контейнер для миниатюр
	QGridLayout *previewLayout;     // Layout для размещения миниатюр

	// Данные текущей папки
	QString currentFolder;          // Текущая просматриваемая папка
	QVector<QString> currentFiles;  // Файлы в текущей папке
	QVector<QLabel*> thumbnailLabels; // Ярлыки для превью

	 // Загрузчик превью
	ThumbnailLoader *thumbnailLoader;
	QThread *loaderThread;
};
