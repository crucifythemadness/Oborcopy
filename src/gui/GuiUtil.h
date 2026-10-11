#pragma once

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QString>
#include <QStringList>

class QFrame;

namespace obor::GuiUtil {

QString formatBytes(qint64 bytes);
QString formatDuration(qint64 seconds);
QString formatCount(qint64 count);

// "*.tmp; Thumbs.db" <-> {"*.tmp", "Thumbs.db"}
QStringList splitPatterns(const QString &text);
QString joinPatterns(const QStringList &patterns);

QFrame *makeCard(QWidget *parent = nullptr);
QLabel *makeLabel(const QString &text, const QString &objectName, QWidget *parent = nullptr);
// Card with a big number and a caption, e.g. "37 / copied".
QFrame *makeStatTile(const QString &value, const QString &caption, const QColor &color,
                     QWidget *parent = nullptr);
// Coloured message strip with an icon (warnings, errors, hints).
QFrame *makeBanner(const QIcon &icon, const QString &html, const QColor &color,
                   QWidget *parent = nullptr);
// Re-applies the style sheet after a dynamic property changed.
void repolish(QWidget *widget);
void openInFileManager(const QString &path);
void openFile(const QString &path);
// Native path with invisible break opportunities after separators, so long
// paths can wrap inside labels.
QString displayPath(const QString &path);

} // namespace obor::GuiUtil

namespace obor {

// Label that shortens long paths in the middle ("C:/Music/…/Track.flac").
class ElidedLabel : public QLabel
{
public:
    explicit ElidedLabel(QWidget *parent = nullptr);
    void setFullText(const QString &text);
    QString fullText() const { return m_fullText; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateElided();
    QString m_fullText;
};

} // namespace obor
