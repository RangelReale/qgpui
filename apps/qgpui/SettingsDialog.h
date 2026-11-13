#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;

// File > Settings: preferences kept in QSettings.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    // Keep the headset's screen on while an install runs (installer::KeepAwake). Off by default.
    static bool keepAwakeWhileInstalling();
    static void setKeepAwakeWhileInstalling(bool on);

    void accept() override;

private:
    void chooseFilesFolder();

    QLineEdit* m_filesFolder;
    QCheckBox* m_keepAwake;
};
