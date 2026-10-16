#include "PathPicker.h"

#include "FsInfo.h"
#include "GuiUtil.h"
#include "IconFactory.h"

#include <QDir>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace obor {

PathPicker::PathPicker(const QString &caption, QWidget *parent)
    : QWidget(parent)
    , m_caption(caption)
    , m_edit(new QLineEdit(this))
    , m_info(GuiUtil::makeLabel(QString(), QStringLiteral("Muted"), this))
    , m_infoTimer(new QTimer(this))
{
    auto *browse = new QToolButton(this);
    browse->setIcon(IconFactory::icon(IconKind::Folder));
    browse->setToolTip(tr("Choose folder…"));
    m_edit->setClearButtonEnabled(true);
    m_edit->installEventFilter(this);
    m_infoTimer->setSingleShot(true);
    m_infoTimer->setInterval(400);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_edit, 1);
    row->addWidget(browse);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    layout->addWidget(GuiUtil::makeLabel(caption, QStringLiteral("SectionTitle"), this));
    layout->addLayout(row);
    layout->addWidget(m_info);

    connect(browse, &QToolButton::clicked, this, &PathPicker::browse);
    connect(m_edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_infoTimer->start();
        emit pathChanged(text);
    });
    connect(m_infoTimer, &QTimer::timeout, this, &PathPicker::updateInfo);
}

QString PathPicker::path() const
{
    return QDir::fromNativeSeparators(m_edit->text().trimmed());
}

void PathPicker::setPath(const QString &path)
{
    const QSignalBlocker blocker(m_edit);
    m_edit->setText(QDir::toNativeSeparators(path));
    updateInfo();
}

void PathPicker::setPlaceholder(const QString &text)
{
    m_edit->setPlaceholderText(text);
}

void PathPicker::browse()
{
    const QString start = path().isEmpty() ? QDir::homePath() : path();
    const QString dir = GuiUtil::chooseDirectory(this, m_caption, start);
    if (!dir.isEmpty())
        m_edit->setText(QDir::toNativeSeparators(dir));
}

void PathPicker::updateInfo()
{
    const QString p = path();
    if (p.isEmpty()) {
        m_info->setText(tr("No folder selected"));
        return;
    }
    const VolumeInfo volume = FsInfo::query(p);
    QString text;
    if (volume.valid) {
        text = tr("%1 · %2 free of %3")
                   .arg(volume.fileSystem, GuiUtil::formatBytes(volume.bytesFree),
                        GuiUtil::formatBytes(volume.bytesTotal));
        if (volume.fatLike)
            text += tr(" · 2 s timestamps");
    }
    if (!QFileInfo(p).isDir())
        text += (text.isEmpty() ? QString() : QStringLiteral(" · ")) + tr("folder does not exist yet");
    m_info->setText(text);
}

bool PathPicker::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_edit)
        return QWidget::eventFilter(watched, event);
    if (event->type() == QEvent::DragEnter) {
        auto *drag = static_cast<QDragEnterEvent *>(event);
        if (drag->mimeData()->hasUrls()) {
            drag->acceptProposedAction();
            return true;
        }
    } else if (event->type() == QEvent::Drop) {
        const auto urls = static_cast<QDropEvent *>(event)->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            QFileInfo info(urls.first().toLocalFile());
            m_edit->setText(QDir::toNativeSeparators(info.isDir() ? info.absoluteFilePath()
                                                                  : info.absolutePath()));
        }
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace obor
