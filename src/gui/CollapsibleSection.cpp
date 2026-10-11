#include "CollapsibleSection.h"

#include "GuiUtil.h"

#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace obor {

CollapsibleSection::CollapsibleSection(const QString &title, const QString &subtitle,
                                       QWidget *parent)
    : QFrame(parent)
    , m_header(new QToolButton(this))
    , m_content(new QWidget(this))
{
    setObjectName(QStringLiteral("Card"));
    m_header->setText(title);
    m_header->setCheckable(true);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setArrowType(Qt::RightArrow);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont font = m_header->font();
    font.setBold(true);
    m_header->setFont(font);

    auto *subtitleLabel = GuiUtil::makeLabel(subtitle, QStringLiteral("Muted"), this);
    subtitleLabel->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->addWidget(m_header);
    layout->addWidget(subtitleLabel);
    layout->addWidget(m_content);
    m_content->setVisible(false);

    connect(m_header, &QToolButton::toggled, this, &CollapsibleSection::setExpanded);
}

void CollapsibleSection::setContentLayout(QLayout *layout)
{
    layout->setContentsMargins(4, 8, 4, 4);
    m_content->setLayout(layout);
}

void CollapsibleSection::setExpanded(bool expanded)
{
    m_header->setChecked(expanded);
    m_header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_content->setVisible(expanded);
}

bool CollapsibleSection::isExpanded() const
{
    return m_content->isVisible();
}

} // namespace obor
