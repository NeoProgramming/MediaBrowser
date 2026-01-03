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
#include "ui_mediabrowser.h"

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
	void loadThumbnails(const QString& folderPath);
	void onThumbnailLoaded(int index, const QPixmap& pixmap);
	
private:
	void setupUI();
	void setupMenu();
	void initThumbnailLoader();
	void clearThumbnails();

	QWidget *centralWidget;
	QScrollArea *scrollArea;
	QWidget *thumbnailContainer;
	QGridLayout *thumbnailLayout;

	QString currentFolder;
	QVector<QString> currentFiles;
	QVector<QLabel*> thumbnailLabels;

	ThumbnailLoader *thumbnailLoader;
	QThread *loaderThread;
private:
    Ui::MediaBrowserClass ui;
};
