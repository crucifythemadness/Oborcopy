#include "ModeCard.h"

#include "GuiUtil.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>

namespace obor {

ModeCard::ModeCard(IconKind icon, const QString &title, const QString &description,
                   QWidget *parent)
    : QFrame(parent)
    , m_badge(new QLabel(this))
{
    setObjectName(QStringLiteral("ModeCard"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setProperty("checked", false);

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(IconFactory::icon(icon, QColor(0x3D, 0x7E, 0xF5)).pixmap(34, 34));
    iconLabel->setAlignment(Qt::AlignTop);

    auto *titleLabel = GuiUtil::makeLabel(title, QStringLiteral("SectionTitle"), this);
    auto *descLabel = GuiUtil::makeLabel(description, QStringLiteral("Muted"), this);
    descLabel->setWordWrap(true);
    m_badge->setVisible(false);

    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(titleLabel);
    titleRow->addWidget(m_badge);
    titleRow->addStretch();

    auto *text = new QVBoxLayout;
    text->addLayout(titleRow);
    text->addWidget(descLabel);
    text->addStretch();

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(12);
    layout->addWidget(iconLabel);
    layout->addLayout(text, 1);
}

void ModeCard::setChecked(bool checked)
{
    if (m_checked == checked)
        return;
    m_checked = checked;
    setProperty("checked", checked);
    GuiUtil::repolish(this);
}

void ModeCard::setBadge(const QString &text, const QColor &color)
{
    m_badge->setText(text);
    m_badge->setStyleSheet(QStringLiteral("color: white; background: %1; border-radius: 8px;"
                                          " padding: 1px 8px; font-size: 8pt; font-weight: 600;")
                               .arg(color.name()));
    m_badge->setVisible(!text.isEmpty());
}

void ModeCard::mousePressEvent(QMouseEvent *event)
{
    QFrame::mousePressEvent(event);
    emit clicked();
}

void ModeCard::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return) {
        emit clicked();
        return;
    }
    QFrame::keyPressEvent(event);
}

} // namespace obor
