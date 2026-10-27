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
        // The remembered disk belongs to the previous folder; the disk of the
        // new one is learnt as soon as it is reachable.
        if (!m_following && !m_knownVolume.isEmpty()) {
            m_knownVolume = VolumeRef();
            emit knownVolumeChanged();
        }
        m_infoTimer->start();
        emit pathChanged(text);
    });
    connect(m_info, &QLabel::linkActivated, this, [this](const QString &link) {
        if (link == QLatin1String("follow"))
            m_edit->setText(QDir::toNativeSeparators(m_suggestion)); // like choosing it by hand
        else
            useOtherDisk();
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

void PathPicker::setKnownVolume(const VolumeRef &ref)
{
    m_knownVolume = ref;
}

// "Use OTHER for this job": the disk found at the old place becomes the job's disk.
void PathPicker::useOtherDisk()
{
    m_knownVolume = VolumeCheck::learn(VolumeRef(), m_otherDisk);
    emit knownVolumeChanged();
    startCheck();
}

void PathPicker::browse()
{
    const QString start = path().isEmpty() ? QDir::homePath() : path();
    const QString dir = GuiUtil::chooseDirectory(this, m_caption, start);
    if (!dir.isEmpty())
        m_edit->setText(QDir::toNativeSeparators(dir));
}

PathPicker::CheckResult PathPicker::check(const QString &path, bool creatable, const VolumeRef &ref)
{
    CheckResult result;
    result.disk = VolumeCheck::resolve(path, ref);
    using Status = VolumeCheck::Status;
    if (result.disk.status == Status::WrongDisk) {
        result.availability = Availability::WrongDisk;
        result.volume = result.disk.volume;
        return result;
    }
    if (result.disk.status == Status::Ambiguous || result.disk.status == Status::Missing)
        return result; // Offline, whatever happens to be at the old place
    // For a moved disk, check the folder at its new place.
    const QString effective = result.disk.path;
    result.volume = FsInfo::query(effective);
    const QFileInfo info(effective);
    if (info.isDir()) {
        result.availability = Availability::Online;
    } else if (creatable && !info.exists() && QFileInfo(info.absolutePath()).isDir()) {
        result.availability = Availability::WillBeCreated;
    } else {
        result.availability = Availability::Offline;
        result.notAFolder = info.exists();
        // The disk is not known yet (e.g. a job from an older version): look
        // for the folder under another drive letter to offer it.
        if (!result.notAFolder && result.disk.status == Status::NoIdentity)
            result.suggestion = VolumeCheck::findElsewhere(path, &result.suggestionVolume);
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
    const VolumeRef ref = m_knownVolume;
    std::thread([guard, generation, p, creatable, ref]() {
        const CheckResult result = check(p, creatable, ref);
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
    const bool moved = result.disk.status == VolumeCheck::Status::Moved;
    if (moved && !isEnabled())
        return; // a sync is running; follow the disk after it
    m_checkedPath = moved ? result.disk.path : path;
    m_notAFolder = result.notAFolder;
    m_diskStatus = result.disk.status;
    m_otherDisk = result.availability == Availability::WrongDisk ? result.volume : VolumeInfo();
    m_suggestion = result.suggestion;
    m_suggestionVolume = result.suggestionVolume;
    if (moved) {
        m_following = true;
        m_edit->setText(QDir::toNativeSeparators(result.disk.path)); // saves the job
        m_following = false;
    }
    // Only a folder that is (or will be) there identifies its disk.
    if (result.availability == Availability::Online || result.availability == Availability::WillBeCreated) {
        const VolumeRef base = result.disk.status == VolumeCheck::Status::NoIdentity ? VolumeRef()
                                                                                    : m_knownVolume;
        const VolumeRef learnt = VolumeCheck::learn(base, result.volume);
        if (learnt != m_knownVolume) {
            m_knownVolume = learnt;
            emit knownVolumeChanged();
        }
    }
    setAvailability(result.availability, result.volume);
    if (moved)
        emit volumeMoved(m_knownVolume.label, result.disk.volume.rootPath);
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
    QString rich; // the text with a link, if any
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
        if (m_notAFolder) {
            text = tr("Not a folder");
        } else if (volume.valid) {
            // A drive is mounted but the folder is missing – possibly the wrong disk.
            text = FsInfo::volumeName(volume) + QStringLiteral(" · ")
                   + (m_creatable ? tr("Offline – the parent folder was not found")
                                  : tr("Offline – folder not found"));
        } else if (m_diskStatus == VolumeCheck::Status::Ambiguous) {
            text = tr("Several connected disks are named %1 – disconnect all but one").arg(m_knownVolume.label);
        } else if (!m_knownVolume.isEmpty()) {
            text = tr("Offline – connect %1").arg(m_knownVolume.label);
            tip = tr("The disk %1 is not connected or not reachable").arg(m_knownVolume.label);
        } else if (!m_suggestion.isEmpty()) {
            const QString found = QDir::toNativeSeparators(m_suggestion);
            text = tr("Offline – found on %1").arg(FsInfo::volumeName(m_suggestionVolume));
            tip = tr("The folder was not found, but %1 exists. Check that it is the right folder "
                     "before you use it.")
                      .arg(found);
            rich = text.toHtmlEscaped() + QStringLiteral(" · <a href=\"follow\">%1</a>")
                                              .arg(tr("Use %1").arg(found).toHtmlEscaped());
        } else {
            text = tr("Offline – the drive or network share is not reachable");
        }
        if (tip.isEmpty())
            tip = text;
        break;
    case Availability::WrongDisk: {
        color = c.danger;
        const QString other = volume.label.isEmpty() ? tr("this disk") : volume.label;
        text = tr("%1 is connected, not %2").arg(FsInfo::volumeName(volume), m_knownVolume.label);
        tip = tr("Another disk is connected where %1 was. Oborcopy does not sync to it unless you "
                 "choose it for this job.")
                  .arg(m_knownVolume.label);
        rich = text.toHtmlEscaped() + QStringLiteral(" · <a href=\"use\">%1</a>")
                                          .arg(tr("Use %1 for this job").arg(other).toHtmlEscaped());
        break;
    }
    }
    const bool problem = availability == Availability::Offline || availability == Availability::WrongDisk;
    m_dot->setPixmap(IconFactory::dot(color).pixmap(kDotSize, kDotSize));
    m_dot->setToolTip(tip);
    // Rich text only for a line with a link; folder names stay plain otherwise.
    m_info->setTextFormat(rich.isEmpty() ? Qt::PlainText : Qt::RichText);
    m_info->setText(rich.isEmpty() ? text : rich);
    m_info->setToolTip(volume.valid ? QDir::toNativeSeparators(volume.rootPath) : QString());
    m_info->setStyleSheet(problem ? QStringLiteral("color: %1;").arg(c.danger.name()) : QString());
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
