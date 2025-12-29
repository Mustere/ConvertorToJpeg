#include "settingsdialog.h"
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QThread>
#include <QHBoxLayout>

SettingsDialog::SettingsDialog(int currentThreads, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Stream Settings"));

    // Фиксируем размеры окна
    setFixedSize(300, 120); // ширина 300px, высота 120px

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Информация о доступных ядрах
    infoLabel = new QLabel(this);
    int totalCores = QThread::idealThreadCount();
    infoLabel->setText(QString(tr("Available cores: %1")).arg(totalCores));
    layout->addWidget(infoLabel);

    // SpinBox для выбора количества потоков
    spinBox = new QSpinBox(this);
    spinBox->setMinimum(1);
    spinBox->setMaximum(totalCores);
    spinBox->setValue(currentThreads);
    layout->addWidget(spinBox);

    // Кнопка OK
    QPushButton *okButton = new QPushButton("OK", this);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);
}

int SettingsDialog::selectedThreads() const {
    return spinBox->value();
}
