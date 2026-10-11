#pragma once

#include "IconFactory.h"

#include <QFrame>

class QLabel;

namespace obor {

// Large selectable card used for the Update / Mirror choice.
class ModeCard : public QFrame
{
    Q_OBJECT

public:
    ModeCard(IconKind icon, const QString &title, const QString &description,
             QWidget *parent = nullptr);

    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }
    void setBadge(const QString &text, const QColor &color);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool m_checked = false;
    QLabel *m_badge;
};

} // namespace obor
