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
#include "categoriespanel.h"
#include "tagspanel.h"
#include "tagmanager.h"

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

	// Слоты для превью
	void onThumbnailsFinished();
	void onThumbnailLoaderError(const QString& error);
	void onThumbnailClicked(int index, Qt::KeyboardModifiers modifiers);
	void onThumbnailDoubleClicked(int index);
	void onSelectionChanged(const QSet<int>& selectedIndices);
	void onSelectionCleared();

	// Слоты для категорий
	void onMoveClicked(const QString& targetCategory);	
	
	// Слоты для тегов
	void onTagToggled(const QString& tag, bool checked);
	void onAddTagClicked(const QString& newTag);

	// Слоты меню
	void onSelectSourceRoot();
	void onSelectTargetRoot();

private:
	void initPreviewArea();
	void initSidebar();
	void initTagsbar();
	void initMenu();

	void moveCurrentFolder();
	QString findNextUnprocessedDir();
	void loadNextUnprocessedFolder();

	void loadFolderThumbnails(const QString& folderPath);

	void openFile(int index);

	void updateTagsPanel();
	void updateObjectTags(const QSet<QString>& newTags);

	// Настройки
	Settings cfg;

	// UI элементы 
	PreviewArea *previewArea;
	CategoriesPanel *categoriesPanel;
	TagsPanel *tagsPanel;

	// Менеджер тегов
	TagManager *tagManager;

	// Данные текущей папки
	QString currentFolder;          // Текущая просматриваемая папка
	QVector<QString> currentFiles;  // Файлы в текущей папке
	QSet<int> selectedFileIndices;	// Выбранные файлы
	
	 // Загрузчик превью
	ThumbnailLoader *thumbnailLoader;
	QThread *loaderThread;
};
