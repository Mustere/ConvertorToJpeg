#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->checkBox->setChecked(true);
    ui->destLineEdit->setEnabled(false);
    ui->browseDestButton->setEnabled(false);

    threadCount = QThread::idealThreadCount() * 3 / 4;

    connect(ui->checkBox, &QCheckBox::toggled, this, [this](bool checked){
        ui->destLineEdit->setEnabled(!checked);
        ui->browseDestButton->setEnabled(!checked);
        if(checked) {
            QString sourceDir = ui->sourceLineEdit->text();
            if(!sourceDir.isEmpty())
                ui->destLineEdit->setText(sourceDir + "/JPEG");
        }
    });

    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopConversion);
    connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);

    ui->statusLabel->setText("Готов к работе.");
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
        ui->logTextEdit->append(QString("Установлено количество потоков: %1").arg(threadCount));
    }
}

void MainWindow::on_browseSourceButton_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Source Folder");
    if(!dir.isEmpty()) {
        ui->sourceLineEdit->setText(dir);
        if(ui->checkBox->isChecked())
            ui->destLineEdit->setText(dir + "/JPEG");
    }
}

void MainWindow::on_browseDestButton_clicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Folder");
    if(!dir.isEmpty())
        ui->destLineEdit->setText(dir);
}

void MainWindow::stopConversion() {
    stopRequested = true;
    ui->logTextEdit->append("Запрос остановки...");
}

void MainWindow::on_startButton_clicked() {

    stopRequested = false;
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->logTextEdit->clear();

    QString sourceDir = ui->sourceLineEdit->text();
    QString destDir = ui->destLineEdit->text();

    if (sourceDir.isEmpty()) {
        ui->logTextEdit->append("❌ Папка исходных файлов не выбрана.");
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        return;
    }

    if(ui->checkBox->isChecked() || destDir.isEmpty())
        destDir = sourceDir + "/JPEG";

    QDir().mkpath(destDir);

    QDir dir(sourceDir);
    QStringList files = dir.entryList(QStringList() << "*.heic", QDir::Files);
    totalFiles = files.size();
    processedFiles = 0;
    successCount = 0;
    errorCount = 0;

    if (totalFiles == 0) {
        ui->logTextEdit->append("❌ Файлы для обработки не найдены.");
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        return;
    }

    ui->progressBar->setMaximum(totalFiles);
    ui->progressBar->setValue(0);
    ui->logTextEdit->append(QString("Найдено файлов: %1").arg(totalFiles));

    timer.start();

    // Разделяем файлы по потокам
    int chunkSize = totalFiles / threadCount + 1;
    threads.clear();

    for(int i=0; i<threadCount; ++i) {
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
        QString("Обработано %1/%2 | Успешно: %3 | Ошибки: %4 | Время: %5 сек")
            .arg(processedFiles)
            .arg(totalFiles)
            .arg(successCount)
            .arg(errorCount)
            .arg(timer.elapsed()/1000.0, 0, 'f', 1)
        );
}

void MainWindow::handleWorkerFinished() {
    bool allDone = true;
    for (QThread *t : threads) {
        if (t->isRunning()) { allDone = false; break; }
    }
    if(allDone) {
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        ui->statusLabel->setText(
            QString("Готово! Всего: %1 | Успешно: %2 | Ошибки: %3 | Время: %4 сек")
                .arg(totalFiles)
                .arg(successCount)
                .arg(errorCount)
                .arg(timer.elapsed()/1000.0, 0, 'f', 1)
            );
    }
}
