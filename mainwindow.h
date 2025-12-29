#pragma once

#include <QMainWindow>
#include <QThread>
#include <QElapsedTimer>
#include <QVector>

#include "converter.h"
#include "clickablelabel.h"
#include <QFileInfo>

#include <QObject>
#include <QStringList>
#include <QImage>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QImageWriter>

#include <libheif/heif.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE



class Worker : public QObject
{
    Q_OBJECT
public:
    Worker(const QStringList &files,
           const QString &sourceDir,
           const QString &destDir,
           int quality,
           bool *stopRequested,
           QObject *parent = nullptr)
        : QObject(parent),
        m_files(files),
        m_sourceDir(sourceDir),
        m_destDir(destDir),
        m_quality(quality),
        m_stopRequested(stopRequested)
    {}

public slots:
    void process() {
        for (const QString &file : m_files) {
            if (m_stopRequested && *m_stopRequested)
                break;

            QString fullPath;
            QFileInfo fi(file);

            if (fi.isAbsolute())
                fullPath = file;
            else if (!m_sourceDir.isEmpty())
                fullPath = QDir(m_sourceDir).absoluteFilePath(file);
            else
                fullPath = file;

            bool ok = convertToJpeg(fullPath);
            emit fileProcessed(fullPath, ok);
        }

        emit finished();
    }

signals:
    void fileProcessed(const QString &file, bool ok);
    void finished();

private:
    bool convertToJpeg(const QString &filePath) {
        QFileInfo fi(filePath);
        if (!fi.exists())
            return false;

        const QString ext = fi.suffix().toLower();

        // 🔹 1. Сначала Qt (быстро + EXIF)
        if (ext != "avif") {
            QImageReader reader(filePath);
            reader.setAutoTransform(true);
            QImage image = reader.read();
            if (!image.isNull()) {
                return saveJpeg(image, fi);
            }
        }

        // 🔹 2. fallback через libheif (HEIC / HEIF / AVIF)
        if (ext == "heic" || ext == "heif" || ext == "avif") {
            return convertWithLibheif(filePath, fi);
        }

        return false;
    }

    bool convertWithLibheif(const QString &filePath, const QFileInfo &fi) {
        heif_context* ctx = heif_context_alloc();
        if (!ctx)
            return false;

        heif_error err = heif_context_read_from_file(
            ctx, filePath.toUtf8().constData(), nullptr);

        if (err.code != heif_error_Ok) {
            heif_context_free(ctx);
            return false;
        }

        heif_image_handle* handle = nullptr;
        err = heif_context_get_primary_image_handle(ctx, &handle);
        if (err.code != heif_error_Ok) {
            heif_context_free(ctx);
            return false;
        }

        heif_image* img = nullptr;
        err = heif_decode_image(
            handle,
            &img,
            heif_colorspace_RGB,
            heif_chroma_interleaved_RGB,
            nullptr
            );

        if (err.code != heif_error_Ok || !img) {
            heif_image_handle_release(handle);
            heif_context_free(ctx);
            return false;
        }

        int width  = heif_image_get_width(img, heif_channel_interleaved);
        int height = heif_image_get_height(img, heif_channel_interleaved);
        int stride = 0;

        const uint8_t* src =
            heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);

        if (!src) {
            heif_image_release(img);
            heif_image_handle_release(handle);
            heif_context_free(ctx);
            return false;
        }

        // ✅ ГЛУБОКАЯ КОПИЯ (обязательно)
        QImage image(width, height, QImage::Format_RGB888);
        for (int y = 0; y < height; ++y) {
            memcpy(image.scanLine(y), src + y * stride, width * 3);
        }

        bool ok = saveJpeg(image, fi);

        heif_image_release(img);
        heif_image_handle_release(handle);
        heif_context_free(ctx);

        return ok;
    }

    bool saveJpeg(const QImage &image, const QFileInfo &fi) {
        QString outFile =
            QDir(m_destDir).absoluteFilePath(fi.completeBaseName() + ".jpg");

        QImageWriter writer(outFile, "JPG");
        writer.setQuality(m_quality);
        return writer.write(image);
    }

private:
    QStringList m_files;
    QString m_sourceDir;
    QString m_destDir;
    int m_quality;
    bool *m_stopRequested;
};
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    int finishedThreads = 0;

private slots:
    void on_browseSourceButton_clicked();
    void on_browseDestButton_clicked();
    void on_startButton_clicked();
    void stopConversion();
    void openSettingsDialog();

    void handleFileProcessed(const QString &file, bool ok);
    void handleWorkerFinished();
    void showAboutDialog();

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
