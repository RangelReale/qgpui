#include "SettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

namespace {
const QString kKeepAwakeSetting = QStringLiteral("install/keepAwake");
} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_keepAwake(new QCheckBox(tr("Keep the headset's screen on during installs"), this))
{
    setWindowTitle(tr("Settings"));

    m_keepAwake->setChecked(keepAwakeWhileInstalling());
    auto* keepAwakeHelp = new QLabel(tr("A Quest goes to sleep about 20 seconds after it is taken off, and "
                                        "some install steps need it awake. With this on, QGPUI makes the "
                                        "headset act as if it is worn until the install ends."),
                                     this);
    keepAwakeHelp->setWordWrap(true);
    keepAwakeHelp->setContentsMargins(20, 0, 0, 0);
    keepAwakeHelp->setForegroundRole(QPalette::PlaceholderText);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_keepAwake);
    layout->addWidget(keepAwakeHelp);
    layout->addStretch();
    layout->addWidget(buttons);
    resize(440, sizeHint().height());
}

bool SettingsDialog::keepAwakeWhileInstalling()
{
    return QSettings().value(kKeepAwakeSetting, false).toBool();
}

void SettingsDialog::setKeepAwakeWhileInstalling(bool on)
{
    QSettings().setValue(kKeepAwakeSetting, on);
}

void SettingsDialog::accept()
{
    setKeepAwakeWhileInstalling(m_keepAwake->isChecked());
    QDialog::accept();
}
