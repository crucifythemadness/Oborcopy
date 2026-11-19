#include "Language.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

namespace obor::Language {

namespace {

LanguageMode g_mode = LanguageMode::System;
bool g_german = false;

bool systemIsGerman()
{
    const QStringList languages = QLocale::system().uiLanguages();
    return !languages.isEmpty() && languages.first().startsWith(QLatin1String("de"));
}

void load(const QString &resource)
{
    auto *translator = new QTranslator(QCoreApplication::instance());
    if (translator->load(resource))
        QCoreApplication::installTranslator(translator);
    else
        delete translator;
}

} // namespace

void install(LanguageMode mode)
{
    g_mode = mode;
    g_german = mode == LanguageMode::German || (mode == LanguageMode::System && systemIsGerman());
    if (!g_german)
        return; // English texts; numbers and dates keep following the system locale
    // German number/date formats; a German system locale (e.g. de_CH) is kept as it is.
    if (QLocale::system().language() != QLocale::German)
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    load(QStringLiteral(":/i18n/qtbase_de.qm"));
    load(QStringLiteral(":/i18n/oborcopy_de.qm"));
}

LanguageMode mode()
{
    return g_mode;
}

bool isGerman()
{
    return g_german;
}

LanguageMode fromString(const QString &name)
{
    if (name == QLatin1String("en"))
        return LanguageMode::English;
    if (name == QLatin1String("de"))
        return LanguageMode::German;
    return LanguageMode::System;
}

QString toString(LanguageMode mode)
{
    switch (mode) {
    case LanguageMode::English: return QStringLiteral("en");
    case LanguageMode::German: return QStringLiteral("de");
    case LanguageMode::System: break;
    }
    return QStringLiteral("system");
}

} // namespace obor::Language
