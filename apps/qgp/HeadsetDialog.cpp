#include "HeadsetDialog.h"

#include "HeadsetConnection.h"

#include "qadb/Reply.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kStatusIconSize = 16;

QPushButton* plainButton(const QString& text, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setAutoDefault(false);
    return button;
}

} // namespace

HeadsetDialog::HeadsetDialog(HeadsetConnection* connection, QWidget* parent)
    : QDialog(parent)
    , m_connection(connection)
    , m_statusIcon(new QLabel(this))
    , m_statusText(new QLabel(this))
    , m_statusAdvice(new QLabel(this))
    , m_retryAdb(plainButton(tr("Try Again"), this))
    , m_list(new QListWidget(this))
    , m_search(plainButton(tr("&Search Again"), this))
    , m_use(plainButton(tr("&Use This Headset"), this))
    , m_message(new QLabel(this))
    , m_wirelessToggle(new QToolButton(this))
    , m_wireless(new QWidget(this))
    , m_address(new QLineEdit(m_wireless))
    , m_code(new QLineEdit(m_wireless))
    , m_add(plainButton(tr("&Pair"), m_wireless))
{
    setWindowTitle(tr("Headset"));
    setMinimumWidth(460);

    // The connection status, as in the main window's status bar.
    QFont bold = m_statusText->font();
    bold.setBold(true);
    m_statusText->setFont(bold);
    m_statusAdvice->setWordWrap(true);
    auto* statusRow = new QHBoxLayout;
    statusRow->addWidget(m_statusIcon);
    statusRow->addWidget(m_statusText, 1);
    statusRow->addWidget(m_retryAdb);

    // The headsets.
    auto* listLabel = new QLabel(tr("Headsets found:"), this);
    m_list->setIconSize(QSize(kStatusIconSize, kStatusIconSize));
    m_list->setSpacing(2);
    m_list->setMinimumHeight(140);
    m_message->setWordWrap(true);
    m_message->setForegroundRole(QPalette::PlaceholderText);
    auto* listButtons = new QHBoxLayout;
    listButtons->addWidget(m_search);
    listButtons->addStretch(1);
    listButtons->addWidget(m_use);

    // Adding a headset over Wi-Fi, hidden until asked for.
    m_wirelessToggle->setText(tr("Add a headset over &Wi-Fi"));
    m_wirelessToggle->setCheckable(true);
    m_wirelessToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_wirelessToggle->setArrowType(Qt::RightArrow);
    m_wirelessToggle->setAutoRaise(true);

    auto* help = new QLabel(tr("On the headset, turn on <b>Wireless debugging</b> and choose <b>Pair device with "
                               "pairing code</b>. Type the IP address and port, and the code it shows.<br>"
                               "To connect a headset that is already paired, leave the code empty."),
                            m_wireless);
    help->setWordWrap(true);
    m_address->setPlaceholderText(tr("For example 192.168.1.20:37255"));
    m_code->setPlaceholderText(tr("6 digits"));
    m_code->setMaxLength(6);
    m_code->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]*")), m_code));
    auto* form = new QFormLayout;
    form->addRow(tr("&Address:"), m_address);
    form->addRow(tr("Pairing &code:"), m_code);
    auto* addRow = new QHBoxLayout;
    addRow->addStretch(1);
    addRow->addWidget(m_add);
    auto* wirelessLayout = new QVBoxLayout(m_wireless);
    wirelessLayout->setContentsMargins(16, 0, 0, 0);
    wirelessLayout->addWidget(help);
    wirelessLayout->addLayout(form);
    wirelessLayout->addLayout(addRow);
    m_wireless->hide();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setAutoDefault(false);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(statusRow);
    layout->addWidget(m_statusAdvice);
    layout->addSpacing(8);
    layout->addWidget(listLabel);
    layout->addWidget(m_list, 1);
    layout->addLayout(listButtons);
    layout->addWidget(m_message);
    layout->addSpacing(8);
    layout->addWidget(m_wirelessToggle);
    layout->addWidget(m_wireless);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_retryAdb, &QPushButton::clicked, m_connection, &HeadsetConnection::retryAdb);
    connect(m_search, &QPushButton::clicked, this, &HeadsetDialog::search);
    connect(m_use, &QPushButton::clicked, this, &HeadsetDialog::useSelected);
    connect(m_list, &QListWidget::itemActivated, this, &HeadsetDialog::useSelected);
    connect(m_list, &QListWidget::currentItemChanged, this, &HeadsetDialog::updateButtons);
    connect(m_wirelessToggle, &QToolButton::toggled, this, [this](bool on) {
        m_wirelessToggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        m_wireless->setVisible(on);
        if (on)
            m_address->setFocus();
        adjustSize();
    });
    connect(m_address, &QLineEdit::textChanged, this, &HeadsetDialog::updateButtons);
    connect(m_code, &QLineEdit::textChanged, this, &HeadsetDialog::updateButtons);
    connect(m_address, &QLineEdit::returnPressed, this, &HeadsetDialog::addWireless);
    connect(m_code, &QLineEdit::returnPressed, this, &HeadsetDialog::addWireless);
    connect(m_add, &QPushButton::clicked, this, &HeadsetDialog::addWireless);

    connect(m_connection, &HeadsetConnection::statusChanged, this, &HeadsetDialog::updateStatus);
    connect(m_connection, &HeadsetConnection::devicesChanged, this, &HeadsetDialog::updateList);
    connect(m_connection, &HeadsetConnection::scanFinished, this,
            [this](const QString& message, const QString& pairingAddress) {
                m_message->setText(message);
                if (!pairingAddress.isEmpty() && m_address->text().isEmpty()) {
                    m_address->setText(pairingAddress);
                    m_wirelessToggle->setChecked(true);
                    m_code->setFocus();
                }
                updateButtons();
            });

    updateStatus();
    updateList();
}

void HeadsetDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    search();
}

void HeadsetDialog::updateStatus()
{
    m_statusIcon->setPixmap(HeadsetConnection::statusIcon(m_connection->isReady())
                                .pixmap(QSize(kStatusIconSize, kStatusIconSize), devicePixelRatio()));
    m_statusText->setText(m_connection->statusText());
    const QString advice = m_connection->statusAdvice();
    m_statusAdvice->setText(advice);
    m_statusAdvice->setVisible(!advice.isEmpty());
    const HeadsetConnection::Status status = m_connection->status();
    m_retryAdb->setVisible(status == HeadsetConnection::Status::AdbMissing ||
                           status == HeadsetConnection::Status::AdbFailed);
    updateButtons();
}

void HeadsetDialog::updateList()
{
    const QString selected = m_list->currentItem() ? m_list->currentItem()->data(Qt::UserRole).toString() : QString();
    const QString current = m_connection->currentSerial();

    m_list->clear();
    for (const qadb::DeviceInfo& device : m_connection->devices()) {
        QString title = HeadsetConnection::displayName(device);
        if (device.serial == current)
            title = tr("%1 (in use)").arg(title);
        auto* item = new QListWidgetItem(HeadsetConnection::statusIcon(device.isOnline()),
                                         QStringLiteral("%1\n%2 · %3")
                                             .arg(title, HeadsetConnection::linkName(device),
                                                  HeadsetConnection::stateDescription(device)),
                                         m_list);
        item->setData(Qt::UserRole, device.serial);
        item->setToolTip(device.serial);
        if (device.serial == current) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
        if (device.serial == (selected.isEmpty() ? current : selected))
            m_list->setCurrentItem(item);
    }

    if (m_list->count() == 0) {
        auto* item = new QListWidgetItem(m_connection->isAdbRunning()
                                             ? tr("No headsets yet. Connect one with a USB cable and turn it on.")
                                             : tr("Waiting for adb..."),
                                         m_list);
        item->setFlags(Qt::NoItemFlags);
    }
    updateButtons();
}

void HeadsetDialog::updateButtons()
{
    const QListWidgetItem* item = m_list->currentItem();
    const QString serial = item ? item->data(Qt::UserRole).toString() : QString();
    m_use->setEnabled(!serial.isEmpty() && serial != m_connection->currentSerial());

    const bool running = m_connection->isAdbRunning();
    m_search->setEnabled(running && !m_connection->isScanning());
    m_add->setText(m_code->text().isEmpty() ? tr("&Connect") : tr("&Pair"));
    m_add->setEnabled(running && !m_address->text().trimmed().isEmpty() &&
                      (m_code->text().isEmpty() || m_code->text().size() == 6));
}

void HeadsetDialog::useSelected()
{
    const QListWidgetItem* item = m_list->currentItem();
    const QString serial = item ? item->data(Qt::UserRole).toString() : QString();
    if (serial.isEmpty())
        return;
    m_connection->setCurrentSerial(serial);
}

void HeadsetDialog::search()
{
    if (!m_connection->isAdbRunning() || m_connection->isScanning())
        return;
    m_message->setText(tr("Searching for wireless headsets..."));
    m_connection->scan();
    updateButtons();
}

void HeadsetDialog::addWireless()
{
    if (!m_add->isEnabled())
        return;
    const QString address = m_address->text().trimmed();
    const QString code = m_code->text();
    const bool pairing = !code.isEmpty();

    m_add->setEnabled(false);
    m_message->setText(pairing ? tr("Pairing with %1...").arg(address) : tr("Connecting to %1...").arg(address));
    qadb::Reply* reply = m_connection->addWirelessHeadset(address, code);
    connect(reply, &qadb::Reply::finished, this, [this, reply, pairing] {
        reply->deleteLater();
        if (reply->isSuccess()) {
            m_message->setText(pairing ? tr("Paired. The headset will show up in the list in a moment.")
                                       : tr("Connected. The headset will show up in the list in a moment."));
            m_code->clear();
            m_wirelessToggle->setChecked(false);
        } else {
            m_message->setText(pairing ? tr("Pairing failed: %1").arg(reply->errorString())
                                       : tr("Could not connect: %1").arg(reply->errorString()));
        }
        updateButtons();
    });
}
