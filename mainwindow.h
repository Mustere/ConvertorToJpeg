#pragma once

#include <QMainWindow>
#include <QThread>
#include <QElapsedTimer>
#include <QVector>

#include "heicconverter.h"
#include <QFileInfo>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class Worker : public QObject {
    Q_OBJECT
public:
    Worker(const QStringList &files, const QString &sourceDir, const QString &destDir, int quality, bool *stopFlag)
        : m_files(files), m_sourceDir(sourceDir), m_destDir(destDir), m_quality(quality), m_stopFlag(stopFlag) {}

public slots:
    void process() {
        for (const QString &file : m_files) {
            if (*m_stopFlag) break;

            QString inputFile = m_sourceDir + "/" + file;
            QString outputFile = m_destDir + "/" + QFileInfo(file).completeBaseName() + ".jpg";

            bool ok = m_converter.convertFile(inputFile, outputFile, m_quality);

            emit fileProcessed(file, ok);
        }
        emit finished();
    }

signals:
    void fileProcessed(const QString &file, bool ok);
    void finished();

private:
    QStringList m_files;
    QString m_sourceDir;
    QString m_destDir;
    int m_quality;
    bool *m_stopFlag;
    HeicConverter m_converter;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_browseSourceButton_clicked();
    void on_browseDestButton_clicked();
    void on_startButton_clicked();
    void stopConversion();
    void openSettingsDialog();

    void handleFileProcessed(const QString &file, bool ok);
    void handleWorkerFinished();

private:
    Ui::MainWindow *ui;

    bool stopRequested = false;
    int totalFiles = 0;
    int processedFiles = 0;
    int successCount = 0;
    int errorCount = 0;
    int threadCount;

    QElapsedTimer timer;
    QVector<QThread*> threads;
};
