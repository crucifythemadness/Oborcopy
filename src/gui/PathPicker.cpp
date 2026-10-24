#include "PathPicker.h"

#include "GuiUtil.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPointer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <mutex>
#include <thread>

namespace obor {

namespace {

constexpr int kRecheckMs = 3000;
constexpr int kDotSize = 18;

// Background checks run in detached threads: a check of an unreachable network
// share can hang for a long time and must neither block the window nor delay
// quitting. Results are only posted while the application object still exists.
std::mutex g_appMutex;
bool g_appAlive = true;

void markAppGone()
{
    const std::lock_guard lock(g_appMutex);
    g_appAlive = false;
}

template <typename Fn>
void postToGui(Fn fn)
{
    const std::lock_guard lock(g_appMutex);
    if (g_appAlive && qApp)
        QMetaObject::invokeMethod(qApp, std::move(fn), Qt::QueuedConnection);
}

} // namespace

PathPicker::PathPicker(const QString &caption, QWidget *parent)
    : QWidget(parent)
    , m_caption(caption)
    , m_edit(new QLineEdit(this))
    , m_dot(new QLabel(this))
    , m_info(GuiUtil::makeLabel(QString(), QStringLiteral("Muted"), this))
    , m_infoTimer(new QTimer(this))
    , m_recheckTimer(new QTimer(this))
{
    static const bool registered = [] {
        qAddPostRoutine(markAppGone);
        return true;
    }();
    Q_UNUSED(registered)

    auto *browse = new QToolButton(this);
    browse->setIcon(IconFactory::icon(IconKind::Folder));
    browse->setToolTip(tr("Choose folder…"));
    m_edit->setClearButtonEnabled(true);
    m_edit->installEventFilter(this);
    m_infoTimer->setSingleShot(true);
    m_infoTimer->setInterval(400);
    m_recheckTimer->setInterval(kRecheckMs);

    auto *captionRow = new QHBoxLayout;
    captionRow->setContentsMargins(0, 0, 0, 0);
    captionRow->setSpacing(6);
    captionRow->addWidget(m_dot);
    captionRow->addWidget(GuiUtil::makeLabel(caption, QStringLiteral("SectionTitle"), this));
    captionRow->addStretch();

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_edit, 1);
    row->addWidget(browse);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    layout->addLayout(captionRow);
    layout->addLayout(row);
    layout->addWidget(m_info);

    connect(browse, &QToolButton::clicked, this, &PathPicker::browse);
    connect(m_edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_infoTimer->start();
        emit pathChanged(text);
    });
    connect(m_infoTimer, &QTimer::timeout, this, &PathPicker::startCheck);
    connect(m_recheckTimer, &QTimer::timeout, this, [this]() {
        if (!m_checking && !m_infoTimer->isActive() && !path().isEmpty())
            startCheck();
    });
    m_recheckTimer->start();
    setAvailability(Availability::Empty, VolumeInfo());
}

QString PathPicker::path() const
{
    return QDir::fromNativeSeparators(m_edit->text().trimmed());
}

void PathPicker::setPath(const QString &path)
{
    const QSignalBlocker blocker(m_edit);
    m_edit->setText(QDir::toNativeSeparators(path));
    m_infoTimer->stop();
    startCheck();
}

void PathPicker::setPlaceholder(const QString &text)
{
    m_edit->setPlaceholderText(text);
}

void PathPicker::setCreatable(bool creatable)
{
    m_creatable = creatable;
}

bool PathPicker::isUsable() const
{
    return m_availability == Availability::Online || m_availability == Availability::WillBeCreated;
}

void PathPicker::browse()
{
    const QString start = path().isEmpty() ? QDir::homePath() : path();
    const QString dir = GuiUtil::chooseDirectory(this, m_caption, start);
    if (!dir.isEmpty())
        m_edit->setText(QDir::toNativeSeparators(dir));
}

PathPicker::CheckResult PathPicker::check(const QString &path, bool creatable)
{
    CheckResult result;
    result.volume = FsInfo::query(path);
    const QFileInfo info(path);
    if (info.isDir()) {
        result.availability = Availability::Online;
    } else if (creatable && !info.exists() && QFileInfo(info.absolutePath()).isDir()) {
        result.availability = Availability::WillBeCreated;
    } else {
        result.availability = Availability::Offline;
        result.notAFolder = info.exists();
    }
    return result;
}

void PathPicker::startCheck()
{
    const QString p = path();
    const quint64 generation = ++m_generation;
    if (p.isEmpty()) {
        m_checking = false;
        setAvailability(Availability::Empty, VolumeInfo());
        return;
    }
    // A different path than the one shown: grey until the first answer arrives.
    if (m_checkedPath != p)
        setAvailability(Availability::Checking, VolumeInfo());
    m_checking = true;
    QPointer<PathPicker> guard(this);
    const bool creatable = m_creatable;
    std::thread([guard, generation, p, creatable]() {
        const CheckResult result = check(p, creatable);
        postToGui([guard, generation, p, result]() {
            if (guard)
                guard->applyResult(generation, p, result);
        });
    }).detach();
}

void PathPicker::applyResult(quint64 generation, const QString &path, const CheckResult &result)
{
    if (generation != m_generation)
        return; // the path changed meanwhile; a newer check is on its way
    m_checking = false;
    m_checkedPath = path;
    m_notAFolder = result.notAFolder;
    setAvailability(result.availability, result.volume);
}

void PathPicker::setAvailability(Availability availability, const VolumeInfo &volume)
{
    const ThemeColors &c = Theme::colors();
    QString volumeText;
    if (volume.valid) {
        volumeText = tr("%1 · %2 · %3 free of %4")
                         .arg(FsInfo::volumeName(volume), volume.fileSystem,
                              GuiUtil::formatBytes(volume.bytesFree),
                              GuiUtil::formatBytes(volume.bytesTotal));
        if (volume.fatLike)
            volumeText += tr(" · 2 s timestamps");
    }

    QColor color = c.muted;
    QString text;
    QString tip;
    switch (availability) {
    case Availability::Empty:
        text = tr("No folder selected");
        tip = text;
        break;
    case Availability::Checking:
        text = tr("Checking…");
        tip = tr("Checking whether the folder is reachable…");
        break;
    case Availability::Online:
        color = c.success;
        text = volumeText;
        tip = tr("Online – the folder is reachable");
        break;
    case Availability::WillBeCreated:
        color = c.success;
        text = volumeText + (volumeText.isEmpty() ? QString() : QStringLiteral(" · "))
               + tr("folder does not exist yet – it will be created");
        tip = tr("Online – the folder will be created by the first sync");
        break;
    case Availability::Offline:
        color = c.danger;
        if (m_notAFolder)
            text = tr("Not a folder");
        else if (volume.valid)
            text = m_creatable ? tr("Offline – the parent folder was not found") : tr("Offline – folder not found");
        else
            text = tr("Offline – the drive or network share is not reachable");
        tip = text;
        break;
    }
    m_dot->setPixmap(IconFactory::dot(color).pixmap(kDotSize, kDotSize));
    m_dot->setToolTip(tip);
    m_info->setText(text);
    m_info->setToolTip(volume.valid ? QDir::toNativeSeparators(volume.rootPath) : QString());
    m_info->setStyleSheet(availability == Availability::Offline
                              ? QStringLiteral("color: %1;").arg(c.danger.name())
                              : QString());
    if (availability != m_availability) {
        m_availability = availability;
        emit availabilityChanged(availability);
    }
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
