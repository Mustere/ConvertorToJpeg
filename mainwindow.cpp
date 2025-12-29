#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->checkBox->setChecked(true);
    ui->destLineEdit->setEnabled(false);
    ui->browseDestButton->setEnabled(false);
    ui->stopButton->setEnabled(false);
    threadCount = QThread::idealThreadCount() * 3 / 4;

    connect(ui->checkBox, &QCheckBox::toggled, this, [this](bool checked){
        ui->destLineEdit->setEnabled(!checked);
        ui->browseDestButton->setEnabled(!checked);
        if(checked) {
            QString source = ui->sourceLineEdit->text();
            if(!source.isEmpty())
                ui->destLineEdit->setText(QFileInfo(source.split(";;").first()).absolutePath() + "/JPEG");
        }
    });

    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopConversion);
    connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);

    // About label
    connect(ui->label_3, &ClickableLabel::clicked, this, &MainWindow::showAboutDialog);

    ui->statusLabel->setText(tr("Ready."));
}

MainWindow::~MainWindow() {
    for (QThread *t : threads) t->quit();
    for (QThread *t : threads) t->wait();
    delete ui;
}

void MainWindow::openSettingsDialog() {
    SettingsDialog dlg(threadCount, this);
    if(dlg.exec() == QDialog::Accepted) {
        threadCount = dlg.selectedThreads();
        ui->logTextEdit->append(QString(tr("The number of threads is set: %1")).arg(threadCount));
    }
}

// ---------------- Выбор исходников ----------------
void MainWindow::on_browseSourceButton_clicked()
{
    // Предложим выбор: файлы или папка
    QStringList selectedFiles = QFileDialog::getOpenFileNames(
        this,
        tr("Select File(s) or Cancel to choose folder"),
        QString(),
        tr("Images (*.heic *.heif *.webp *.HEIC *.HEIF *.WEBP *.avif *.AVIF)")
        );

    if(!selectedFiles.isEmpty()) {
        // выбраны файлы
        ui->sourceLineEdit->setText(selectedFiles.join(";;"));
    } else {
        // если пользователь отменил выбор файла — открываем выбор папки
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
        if(!dir.isEmpty()) {
            ui->sourceLineEdit->setText(dir);
        }
    }

    // Автогенерация папки назначения
    if(ui->checkBox->isChecked()) {
        QString firstPath = ui->sourceLineEdit->text().split(";;").first();
        QString destDir;
        QFileInfo fi(firstPath);
        if(fi.isDir())
            destDir = fi.absoluteFilePath() + "/JPEG";
        else
            destDir = fi.absolutePath() + "/JPEG";

        QDir().mkpath(destDir);
        ui->destLineEdit->setText(destDir);
    }
}

void MainWindow::on_browseDestButton_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Folder");
    if(!dir.isEmpty())
        ui->destLineEdit->setText(dir);
}

void MainWindow::stopConversion() {
    stopRequested = true;
    ui->logTextEdit->append(tr("Interrupted"));
}

// ---------------- Начало конвертации ----------------
void MainWindow::on_startButton_clicked() {

    stopRequested = false;
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->logTextEdit->clear();

    QStringList selected = ui->sourceLineEdit->text().split(";;", Qt::SkipEmptyParts);
    if(selected.isEmpty()) {
        ui->logTextEdit->append(tr("❌ The source files/folder are not selected."));
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        return;
    }

    QString destDir = ui->destLineEdit->text();
    if(ui->checkBox->isChecked() || destDir.isEmpty()) {
        QFileInfo fi(selected.first());
        if(fi.isDir())
            destDir = fi.absoluteFilePath() + "/JPEG";
        else
            destDir = fi.absolutePath() + "/JPEG";
    }

    QDir().mkpath(destDir);

    // формируем список файлов
    QStringList files;
    QString sourceDir;

    QFileInfo first(selected.first());
    if(first.isDir()) {
        sourceDir = first.absoluteFilePath();
        QDir dir(sourceDir);
        files = dir.entryList(
            {"*.heic","*.heif","*.webp","*.HEIC","*.HEIF","*.WEBP","*.avif","*.AVIF"},
            QDir::Files
            );
        // делаем пути абсолютными
        for(int i=0;i<files.size();++i)
            files[i] = dir.absoluteFilePath(files[i]);
    } else {
        files = selected; // уже абсолютные пути
        sourceDir.clear();
    }

    totalFiles = files.size();
    processedFiles = 0;
    successCount = 0;
    errorCount = 0;
    finishedThreads = 0;

    if(totalFiles == 0) {
        ui->logTextEdit->append(tr("❌ No files were found for processing."));
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        return;
    }

    ui->progressBar->setMaximum(totalFiles);
    ui->progressBar->setValue(0);
    ui->logTextEdit->append(QString(tr("Files found: %1")).arg(totalFiles));

    timer.start();

    int chunkSize = totalFiles / threadCount + 1;
    threads.clear();

    for(int i=0;i<threadCount;++i) {
        int start = i * chunkSize;
        int end = qMin(start + chunkSize, totalFiles);
        if(start >= end) break;

        QStringList subFiles = files.mid(start, end - start);
        QThread *t = new QThread;
        Worker *worker = new Worker(subFiles, sourceDir, destDir, ui->qualitySpinBox->value(), &stopRequested);
        worker->moveToThread(t);

        connect(t, &QThread::started, worker, &Worker::process);
        connect(worker, &Worker::fileProcessed, this, &MainWindow::handleFileProcessed);
        connect(worker, &Worker::finished, this, &MainWindow::handleWorkerFinished);
        connect(worker, &Worker::finished, worker, &Worker::deleteLater);
        connect(t, &QThread::finished, t, &QThread::deleteLater);

        threads.append(t);
        t->start();
    }
}

void MainWindow::handleFileProcessed(const QString &file, bool ok) {
    processedFiles++;
    if(ok) successCount++; else errorCount++;
    ui->logTextEdit->append(ok ? "✅ " + file : "❌ " + file);
    ui->progressBar->setValue(processedFiles);
    ui->statusLabel->setText(
        QString(tr("Processed %1/%2 | Successful: %3 | Errors: %4 | Time: %5 sec"))
            .arg(processedFiles)
            .arg(totalFiles)
            .arg(successCount)
            .arg(errorCount)
            .arg(timer.elapsed()/1000.0, 0, 'f', 1)
        );
}

void MainWindow::handleWorkerFinished() {
    finishedThreads++;

    if(finishedThreads < threads.size())
        return;

    if(stopRequested){
        ui->logTextEdit->append(tr("The processing was interrupted by the user."));
        ui->progressBar->setValue(0);
    }
    else
        ui->logTextEdit->append(tr("Processing is completed."));

    ui->startButton->setEnabled(true);
    ui->stopButton->setEnabled(false);

    for(QThread *t : threads) {
        t->quit();
        t->wait();
        t->deleteLater();
    }
    threads.clear();
}

void MainWindow::showAboutDialog() {
    QMessageBox::about(
        this,
        tr("About"),
        tr("Converter To JPEG\nVersion: 1.1.0\nConverting images to JPEG\nSupports HEIC, WEBP, AVIF\nby Mustere's solutions 2025")
        );
}
