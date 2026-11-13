#include "SettingsDialog.h"

#include "GameDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
const QString kKeepAwakeSetting = QStringLiteral("install/keepAwake");
} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_filesFolder(new QLineEdit(this))
    , m_keepAwake(new QCheckBox(tr("Keep the headset's screen on during installs"), this))
{
    setWindowTitle(tr("Settings"));

    // The files folder (decision 0009): where QGPUI looks for the files each game needs.
    auto* filesBox = new QGroupBox(tr("Files folder"), this);
    m_filesFolder->setText(QDir::toNativeSeparators(GameDialog::filesRoot()));
    m_filesFolder->setPlaceholderText(tr("None"));
    m_filesFolder->setClearButtonEnabled(true);
    auto* browse = new QPushButton(tr("&Browse..."), filesBox);
    connect(browse, &QPushButton::clicked, this, &SettingsDialog::chooseFilesFolder);
    auto* filesHelp = new QLabel(tr("Optional. QGPUI looks here for the files a game needs that are not in your "
                                    "Steam or GOG library, one folder per game. Without it, you choose the files "
                                    "in each game's window."),
                                 filesBox);
    filesHelp->setWordWrap(true);
    filesHelp->setForegroundRole(QPalette::PlaceholderText);
    auto* filesRow = new QHBoxLayout;
    filesRow->addWidget(m_filesFolder, 1);
    filesRow->addWidget(browse);
    auto* filesLayout = new QVBoxLayout(filesBox);
    filesLayout->addLayout(filesRow);
    filesLayout->addWidget(filesHelp);

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

    auto* installBox = new QGroupBox(tr("Installs"), this);
    auto* installLayout = new QVBoxLayout(installBox);
    installLayout->addWidget(m_keepAwake);
    installLayout->addWidget(keepAwakeHelp);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(filesBox);
    layout->addWidget(installBox);
    layout->addStretch();
    layout->addWidget(buttons);
    resize(480, sizeHint().height());
}

bool SettingsDialog::keepAwakeWhileInstalling()
{
    return QSettings().value(kKeepAwakeSetting, false).toBool();
}

void SettingsDialog::setKeepAwakeWhileInstalling(bool on)
{
    QSettings().setValue(kKeepAwakeSetting, on);
}

void SettingsDialog::chooseFilesFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Files Folder"),
                                                             QDir::fromNativeSeparators(m_filesFolder->text()));
    if (!folder.isEmpty())
        m_filesFolder->setText(QDir::toNativeSeparators(folder));
}

void SettingsDialog::accept()
{
    const QString folder = m_filesFolder->text().trimmed();
    GameDialog::setFilesRoot(folder.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(folder)));
    setKeepAwakeWhileInstalling(m_keepAwake->isChecked());
    QDialog::accept();
}
