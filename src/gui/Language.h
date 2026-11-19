#pragma once

#include <QString>

namespace obor {

enum class LanguageMode { System, English, German };

// UI language. Installed once at start-up (before any window is built);
// a change from the View menu takes effect after a restart.
namespace Language {

void install(LanguageMode mode);
LanguageMode mode();     // as chosen in the settings
bool isGerman();         // what is actually shown (System resolved)

LanguageMode fromString(const QString &name); // "system" | "en" | "de"
QString toString(LanguageMode mode);

} // namespace Language
} // namespace obor
