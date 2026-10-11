#pragma once

#include <QFrame>

class QToolButton;
class QVBoxLayout;

namespace obor {

// A card with a clickable header that shows or hides its content.
class CollapsibleSection : public QFrame
{
    Q_OBJECT

public:
    CollapsibleSection(const QString &title, const QString &subtitle, QWidget *parent = nullptr);

    void setContentLayout(QLayout *layout);
    void setExpanded(bool expanded);
    bool isExpanded() const;

private:
    QToolButton *m_header;
    QWidget *m_content;
};

} // namespace obor
