#include "ServiceStatusPill.h"

#include "GuiUtil.h"
#include "Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace obor {

namespace {

constexpr int kHeight = 50;
constexpr int kDotX = 20;
constexpr qreal kDotRadius = 5.0;

} // namespace

ServiceStatusPill::ServiceStatusPill(QWidget *parent)
    : QWidget(parent)
    , m_pulse(new QVariantAnimation(this))
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    setToolTip(tr("Open the Service Manager (Ctrl+M)"));
    m_pulse->setStartValue(0.0);
    m_pulse->setEndValue(1.0);
    m_pulse->setLoopCount(-1);
    connect(m_pulse, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_phase = value.toReal();
        update(QRect(0, 0, kDotX * 2, height()));
    });
    setState(ServiceState());
}

void ServiceStatusPill::setState(const ServiceState &state, const QString &busyText)
{
    m_state = state;
    m_title = busyText.isEmpty() ? state.title() : busyText;
    switch (state.kind) {
    case ServiceState::Kind::Running:
        if (state.status.nextRuns.isEmpty()) {
            m_subtitle = tr("No job scheduled");
        } else {
            const NextRun &next = state.status.nextRuns.first();
            m_subtitle = tr("Next: %1 · %2%3")
                             .arg(next.job, GuiUtil::formatWhen(next.at),
                                  next.attempt > 0 ? tr(" (retry %1)").arg(next.attempt) : QString());
        }
        if (!state.responding)
            m_subtitle = tr("Not responding");
        break;
    case ServiceState::Kind::Busy:
        m_subtitle = tr("since %1").arg(GuiUtil::formatWhen(state.status.currentSince));
        break;
    case ServiceState::Kind::Paused: m_subtitle = tr("No backups start until resumed"); break;
    case ServiceState::Kind::Stopped: m_subtitle = tr("Scheduled backups do not run"); break;
    case ServiceState::Kind::NotInstalled: m_subtitle = tr("Click to schedule backups"); break;
    case ServiceState::Kind::Failed:
    case ServiceState::Kind::Unavailable: m_subtitle = tr("Click for details"); break;
    default: m_subtitle.clear(); break;
    }
    if (state.binaryMismatch && state.isInstalled())
        m_subtitle = tr("Uses another Oborcopy folder – click to update");

    // Pulse only while the service is alive; faster while a backup runs.
    const bool alive = state.kind == ServiceState::Kind::Running || state.kind == ServiceState::Kind::Busy
                       || state.kind == ServiceState::Kind::Starting || state.kind == ServiceState::Kind::Stopping;
    const int duration = state.kind == ServiceState::Kind::Busy ? 1000 : 2200;
    if (alive) {
        if (m_pulse->duration() != duration)
            m_pulse->setDuration(duration);
        if (m_pulse->state() != QAbstractAnimation::Running)
            m_pulse->start();
    } else {
        m_pulse->stop();
        m_phase = 0;
    }
    setAccessibleName(m_title + QStringLiteral(". ") + m_subtitle);
    update();
}

QSize ServiceStatusPill::sizeHint() const
{
    return {220, kHeight};
}

QSize ServiceStatusPill::minimumSizeHint() const
{
    return {150, kHeight};
}

QColor ServiceStatusPill::dotColor() const
{
    const ThemeColors &c = Theme::colors();
    switch (m_state.kind) {
    case ServiceState::Kind::Running: return m_state.responding ? c.success : c.warning;
    case ServiceState::Kind::Busy: return c.accent;
    case ServiceState::Kind::Starting:
    case ServiceState::Kind::Stopping:
    case ServiceState::Kind::Paused: return c.warning;
    case ServiceState::Kind::Failed: return c.danger;
    default: return c.muted;
    }
}

void ServiceStatusPill::paintEvent(QPaintEvent *)
{
    const ThemeColors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QColor dot = dotColor();
    QColor background = c.card;
    if (m_hover)
        background = Theme::isDark() ? background.lighter(118) : background.darker(103);
    p.setPen(QPen(hasFocus() ? c.accent : (m_hover ? dot : c.border), 1.0));
    p.setBrush(background);
    p.drawRoundedRect(r, 12, 12);

    // Status dot with an expanding, fading halo.
    const QPointF center(kDotX, height() / 2.0);
    if (m_pulse->state() == QAbstractAnimation::Running) {
        QColor halo = dot;
        halo.setAlphaF(0.45 * (1.0 - m_phase));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        const qreal radius = kDotRadius + 7.0 * m_phase;
        p.drawEllipse(center, radius, radius);
    }
    QColor soft = dot;
    soft.setAlphaF(0.22);
    p.setPen(Qt::NoPen);
    p.setBrush(soft);
    p.drawEllipse(center, kDotRadius + 2.5, kDotRadius + 2.5);
    p.setBrush(dot);
    p.drawEllipse(center, kDotRadius, kDotRadius);

    // Two lines of text, elided to the available width.
    const int textX = kDotX * 2 - 2;
    const int textWidth = width() - textX - 10;
    QFont titleFont = font();
    titleFont.setWeight(QFont::DemiBold);
    QFont subFont = font();
    subFont.setPointSizeF(font().pointSizeF() * 0.88);
    const QFontMetrics tm(titleFont);
    const QFontMetrics sm(subFont);
    const bool twoLines = !m_subtitle.isEmpty();
    const int total = tm.height() + (twoLines ? sm.height() : 0);
    int y = (height() - total) / 2;
    p.setFont(titleFont);
    p.setPen(c.text);
    p.drawText(QRect(textX, y, textWidth, tm.height()), Qt::AlignLeft | Qt::AlignVCenter,
               tm.elidedText(m_title, Qt::ElideRight, textWidth));
    if (twoLines) {
        y += tm.height();
        p.setFont(subFont);
        p.setPen(c.muted);
        p.drawText(QRect(textX, y, textWidth, sm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   sm.elidedText(m_subtitle, Qt::ElideRight, textWidth));
    }
}

void ServiceStatusPill::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        emit clicked();
}

void ServiceStatusPill::enterEvent(QEnterEvent *event)
{
    m_hover = true;
    update();
    QWidget::enterEvent(event);
}

void ServiceStatusPill::leaveEvent(QEvent *event)
{
    m_hover = false;
    update();
    QWidget::leaveEvent(event);
}

void ServiceStatusPill::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit clicked();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace obor
