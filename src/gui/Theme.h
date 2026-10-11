#pragma once

#include "RunReport.h"

#include <QColor>
#include <QString>

namespace obor {

enum class ThemeMode { System, Light, Dark };

struct ThemeColors
{
    QColor window;
    QColor sidebar;
    QColor card;
    QColor input;
    QColor border;
    QColor text;
    QColor muted;
    QColor accent;
    QColor accentSoft;
    QColor accent2;
    QColor success;
    QColor update;
    QColor danger;
    QColor warning;
};

// Fusion style + custom palette + style sheet, in a light and a dark variant.
namespace Theme {

void apply(ThemeMode mode);
ThemeMode current();
bool isDark();
const ThemeColors &colors();
QColor colorFor(ItemKind kind);

ThemeMode fromString(const QString &name);
QString toString(ThemeMode mode);

} // namespace Theme
} // namespace obor
