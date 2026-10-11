#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace obor::Theme {

namespace {

ThemeMode g_mode = ThemeMode::System;
bool g_dark = false;
ThemeColors g_colors;

ThemeColors lightColors()
{
    return {QColor("#F3F5F9"), QColor("#E9EDF4"), QColor("#FFFFFF"), QColor("#FFFFFF"),
            QColor("#D9DFE9"), QColor("#1C2230"), QColor("#677086"), QColor("#2F6FEB"),
            QColor("#E3ECFD"), QColor("#13B3A3"), QColor("#1E9E5A"), QColor("#2F6FEB"),
            QColor("#D64545"), QColor("#D9822B")};
}

ThemeColors darkColors()
{
    return {QColor("#14171D"), QColor("#101318"), QColor("#1D2129"), QColor("#252A34"),
            QColor("#313846"), QColor("#E6E9EF"), QColor("#98A1B3"), QColor("#4C8DFF"),
            QColor("#1F2E4A"), QColor("#2CD3C1"), QColor("#3CCB7F"), QColor("#5B9BFF"),
            QColor("#FF6B6B"), QColor("#FFB547")};
}

bool systemPrefersDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return QApplication::style()->standardPalette().color(QPalette::Window).lightness() < 128;
#endif
}

QPalette buildPalette(const ThemeColors &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.input);
    p.setColor(QPalette::AlternateBase, c.window);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.card);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::ToolTipBase, c.card);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::PlaceholderText, c.muted);
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::Mid, c.border);
    p.setColor(QPalette::Midlight, c.border);
    p.setColor(QPalette::Dark, c.border.darker(130));
    p.setColor(QPalette::Light, c.card);
    p.setColor(QPalette::Disabled, QPalette::Text, c.muted);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.muted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.muted);
    return p;
}

QString styleSheet(const ThemeColors &c)
{
    QString qss = QStringLiteral(R"(
QMainWindow, QDialog { background: @window; }
QFrame#Sidebar { background: @sidebar; border: none; }
QFrame#Card { background: @card; border: 1px solid @border; border-radius: 12px; }
QFrame#Banner { border-radius: 8px; padding: 6px; }
QLabel#AppTitle { font-size: 16pt; font-weight: 700; }
QLabel#Title { font-size: 14pt; font-weight: 600; }
QLabel#SectionTitle { font-size: 10.5pt; font-weight: 600; }
QLabel#Muted, QLabel#SidebarHeader { color: @muted; }
QLabel#SidebarHeader { font-weight: 600; letter-spacing: 1px; }
QLabel#StatValue { font-size: 16pt; font-weight: 700; }
QPushButton { padding: 6px 14px; border-radius: 8px; border: 1px solid @border; background: @card; }
QPushButton:hover { border-color: @accent; }
QPushButton:disabled { color: @muted; }
QPushButton#Primary { background: @accent; color: white; border: none; font-weight: 600; padding: 8px 22px; }
QPushButton#Primary:hover { background: @accentHover; }
QPushButton#Primary:disabled { background: @border; color: @muted; }
QPushButton#Danger { color: @danger; }
QToolButton { border: none; border-radius: 6px; padding: 4px; }
QToolButton:hover { background: @accentSoft; }
QToolButton#Chip { border: 1px solid @border; border-radius: 11px; padding: 3px 10px; }
QToolButton#Chip:checked { background: @accentSoft; border-color: @accent; }
QLineEdit, QPlainTextEdit, QTextBrowser { border: 1px solid @border; border-radius: 6px; padding: 4px 6px; background: @input; }
QLineEdit:focus, QPlainTextEdit:focus { border-color: @accent; }
QProgressBar { border: none; border-radius: 5px; background: @border; min-height: 10px; max-height: 10px; text-align: center; color: transparent; }
QProgressBar::chunk { border-radius: 5px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @accent, stop:1 @accent2); }
QProgressBar#Small { min-height: 6px; max-height: 6px; border-radius: 3px; }
QProgressBar#Small::chunk { border-radius: 3px; }
QTabWidget::pane { border: none; }
QTabBar::tab { padding: 8px 20px; border: none; border-bottom: 2px solid transparent; color: @muted; background: transparent; }
QTabBar::tab:selected { color: @text; border-bottom-color: @accent; font-weight: 600; }
QListWidget#JobList { border: none; background: transparent; outline: none; }
QListWidget#JobList::item { padding: 9px 10px; border-radius: 8px; margin: 1px 0; }
QListWidget#JobList::item:hover { background: @accentSoft; }
QListWidget#JobList::item:selected { background: @accent; color: white; }
QFrame#ModeCard { background: @card; border: 2px solid @border; border-radius: 12px; }
QFrame#ModeCard:hover { border-color: @accentHover; }
QFrame#ModeCard[checked="true"] { border-color: @accent; background: @accentSoft; }
QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QTreeView, QTableWidget { border: 1px solid @border; border-radius: 8px; background: @input; alternate-background-color: @window; }
QHeaderView::section { background: @card; border: none; border-bottom: 1px solid @border; padding: 5px; font-weight: 600; }
QSplitter::handle { background: transparent; }
)");
    const std::pair<const char *, QColor> tokens[] = {
        {"@window", c.window}, {"@sidebar", c.sidebar}, {"@card", c.card},
        {"@input", c.input},   {"@border", c.border},   {"@text", c.text},
        {"@muted", c.muted},   {"@accentSoft", c.accentSoft}, {"@accentHover", c.accent.lighter(115)},
        {"@accent2", c.accent2}, {"@accent", c.accent}, {"@danger", c.danger}};
    for (const auto &[token, color] : tokens)
        qss.replace(QLatin1String(token), color.name());
    return qss;
}

} // namespace

void apply(ThemeMode mode)
{
    g_mode = mode;
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    g_dark = mode == ThemeMode::Dark || (mode == ThemeMode::System && systemPrefersDark());
    g_colors = g_dark ? darkColors() : lightColors();
    QApplication::setPalette(buildPalette(g_colors));
    qApp->setStyleSheet(styleSheet(g_colors));
}

ThemeMode current()
{
    return g_mode;
}

bool isDark()
{
    return g_dark;
}

const ThemeColors &colors()
{
    return g_colors;
}

QColor colorFor(ItemKind kind)
{
    switch (kind) {
    case ItemKind::Copied:
    case ItemKind::DirCreated: return g_colors.success;
    case ItemKind::Updated: return g_colors.update;
    case ItemKind::Deleted:
    case ItemKind::DirDeleted: return g_colors.danger;
    case ItemKind::Error: return g_colors.warning;
    case ItemKind::Skipped: return g_colors.muted;
    }
    return g_colors.text;
}

ThemeMode fromString(const QString &name)
{
    if (name == QLatin1String("light"))
        return ThemeMode::Light;
    if (name == QLatin1String("dark"))
        return ThemeMode::Dark;
    return ThemeMode::System;
}

QString toString(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::Light: return QStringLiteral("light");
    case ThemeMode::Dark: return QStringLiteral("dark");
    case ThemeMode::System: break;
    }
    return QStringLiteral("system");
}

} // namespace obor::Theme
