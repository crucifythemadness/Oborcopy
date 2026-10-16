#pragma once

#include "ServiceController.h"

#include <QWidget>

class QVariantAnimation;

namespace obor {

// Sidebar indicator for the scheduler service: a rounded pill with a softly
// pulsing status dot and two lines of text, e.g.
//   ● Service running
//     Next: Music Backup · today 23:30
// Green = running, blue (faster pulse) = backing up, amber = paused,
// grey = stopped / not set up, red = error. Click opens the Service Manager.
class ServiceStatusPill : public QWidget
{
    Q_OBJECT

public:
    explicit ServiceStatusPill(QWidget *parent = nullptr);

    void setState(const ServiceState &state, const QString &busyText = QString());
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QColor dotColor() const;

    ServiceState m_state;
    QString m_title;
    QString m_subtitle;
    bool m_hover = false;
    qreal m_phase = 0;
    QVariantAnimation *m_pulse;
};

} // namespace obor
