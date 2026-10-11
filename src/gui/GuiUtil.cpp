#include "GuiUtil.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLocale>
#include <QRegularExpression>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

namespace obor::GuiUtil {

QString formatBytes(qint64 bytes)
{
    return QLocale().formattedDataSize(qMax<qint64>(0, bytes), 1, QLocale::DataSizeTraditionalFormat);
}

QString formatDuration(qint64 seconds)
{
    if (seconds < 0)
        return QStringLiteral("–");
    const qint64 h = seconds / 3600;
    const qint64 m = (seconds % 3600) / 60;
    const qint64 s = seconds % 60;
    if (h > 0)
        return QStringLiteral("%1 h %2 min").arg(h).arg(m);
    if (m > 0)
        return QStringLiteral("%1 min %2 s").arg(m).arg(s);
    return QStringLiteral("%1 s").arg(s);
}

QString formatCount(qint64 count)
{
    return QLocale().toString(count);
}

QStringList splitPatterns(const QString &text)
{
    static const QRegularExpression separators(QStringLiteral("[;,\\n]"));
    QStringList result;
    for (const QString &part : text.split(separators, Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty())
            result << trimmed;
    }
    return result;
}

QString joinPatterns(const QStringList &patterns)
{
    return patterns.join(QStringLiteral("; "));
}

QFrame *makeCard(QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("Card"));
    return card;
}

QLabel *makeLabel(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    return label;
}

QFrame *makeStatTile(const QString &value, const QString &caption, const QColor &color,
                     QWidget *parent)
{
    QFrame *tile = makeCard(parent);
    auto *valueLabel = makeLabel(value, QStringLiteral("StatValue"), tile);
    valueLabel->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
    auto *layout = new QVBoxLayout(tile);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(0);
    layout->addWidget(valueLabel);
    layout->addWidget(makeLabel(caption, QStringLiteral("Muted"), tile));
    return tile;
}

QFrame *makeBanner(const QIcon &icon, const QString &html, const QColor &color, QWidget *parent)
{
    auto *banner = new QFrame(parent);
    banner->setObjectName(QStringLiteral("Banner"));
    QColor background = color;
    background.setAlpha(38);
    banner->setStyleSheet(QStringLiteral("QFrame#Banner { background: rgba(%1,%2,%3,%4); "
                                         "border: 1px solid %5; }")
                              .arg(background.red()).arg(background.green())
                              .arg(background.blue()).arg(background.alpha()).arg(color.name()));
    auto *iconLabel = new QLabel(banner);
    iconLabel->setPixmap(icon.pixmap(22, 22));
    iconLabel->setAlignment(Qt::AlignTop);
    auto *text = new QLabel(html, banner);
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *layout = new QHBoxLayout(banner);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->addWidget(iconLabel);
    layout->addWidget(text, 1);
    return banner;
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void openInFileManager(const QString &path)
{
    const QFileInfo info(path);
    const QString target = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(target));
}

void openFile(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString displayPath(const QString &path)
{
    QString native = QDir::toNativeSeparators(path);
    const QChar separator = QDir::separator();
    native.replace(separator, QString(separator) + QChar(0x200B));
    return native;
}

} // namespace obor::GuiUtil

namespace obor {

ElidedLabel::ElidedLabel(QWidget *parent)
    : QLabel(parent)
{
    setMinimumWidth(50);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

void ElidedLabel::setFullText(const QString &text)
{
    m_fullText = text;
    setToolTip(text);
    updateElided();
}

void ElidedLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    updateElided();
}

void ElidedLabel::updateElided()
{
    setText(fontMetrics().elidedText(m_fullText, Qt::ElideMiddle, width()));
}

} // namespace obor
