#pragma once

#include <QDialog>

class HeadsetConnection;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QToolButton;
class QWidget;

// Lets the user choose the headset to use: lists the connected headsets, searches for wireless ones
// and pairs new ones.
class HeadsetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HeadsetDialog(HeadsetConnection* connection, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void updateStatus();
    void updateList();
    void updateButtons();
    void useSelected();
    void search();
    void addWireless();

    HeadsetConnection* m_connection;
    QLabel* m_statusIcon;
    QLabel* m_statusText;
    QLabel* m_statusAdvice;
    QPushButton* m_retryAdb;
    QListWidget* m_list;
    QPushButton* m_search;
    QPushButton* m_use;
    QLabel* m_message;
    QToolButton* m_wirelessToggle;
    QWidget* m_wireless;
    QLineEdit* m_address;
    QLineEdit* m_code;
    QPushButton* m_add;
};
