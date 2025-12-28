#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QSpinBox;
class QLabel;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(int currentThreads, QWidget *parent = nullptr);
    int selectedThreads() const;

private:
    QSpinBox *spinBox;
    QLabel *infoLabel;
};

#endif // SETTINGSDIALOG_H
