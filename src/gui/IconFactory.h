#pragma once

#include <QColor>
#include <QIcon>

namespace obor {

enum class IconKind {
    Play,
    Pause,
    Stop,
    Preview,
    Folder,
    Add,
    Duplicate,
    Edit,
    Trash,
    History,
    Info,
    Warning,
    Error,
    Check,
    UpdateMode,
    MirrorMode,
    Document,
    Arrow
};

// Vector icons painted at runtime, so they follow the theme colours and stay
// sharp on high-DPI screens without needing image files.
namespace IconFactory {

QIcon icon(IconKind kind, const QColor &color = QColor());
QIcon dot(const QColor &color);
QIcon appIcon();

} // namespace IconFactory
} // namespace obor
