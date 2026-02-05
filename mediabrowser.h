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
	void selectFolder();
	void selectSourceFolder();
	void selectTargetFolder();
	void loadThumbnails(const QString& folderPath);
	void onThumbnailLoaded(int index, const QPixmap& pixmap);
	void onThumbnailsFinished();
	void loadNextUnprocessedDir();
private:
	void setupUI();
	void setupMenu();
	void initThumbnailLoader();
	void clearThumbnails();
	void moveCurrentFolder();
	QString findNextUnprocessedDir();

	// UI элементы
	QWidget *centralWidget;
	QScrollArea *scrollArea;
	QWidget *thumbnailContainer;
	QGridLayout *thumbnailLayout;
	QTreeWidget *targetTree;
	QLineEdit *sourcePathEdit;
	QLineEdit *targetPathEdit;
	QPushButton *moveButton;
	QLabel *statusLabel;
	QLabel *ffmpegStatusLabel;
	QVector<QLabel*> thumbnailLabels;
	
	// Данные
	QString currentFolder;
	QVector<QString> currentFiles;
	Settings cfg;
	QStringList m_pendingFolders;

	// Загрузчик превью
	ThumbnailLoader *thumbnailLoader;
	QThread *loaderThread;
};
