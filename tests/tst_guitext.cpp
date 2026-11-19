#include "GuiText.h"
#include "JobValidator.h"
#include "Language.h"
#include "SyncControl.h"
#include "SyncEngine.h"
#include "TestUtil.h"

#include <QTemporaryDir>
#include <QTest>

using namespace obor;

// The GUI shows core messages (English, as in the log) in German by matching
// them against known patterns. These tests use the real core output where
// possible, so a reworded core message that no longer matches is noticed.
class TestGuiText : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;

private slots:
    void initTestCase()
    {
        QVERIFY(m_root.isValid());
        qputenv("OBORCOPY_CONFIG_DIR", (m_root.path() + "/config").toUtf8());
        Language::install(LanguageMode::German);
        QVERIFY(Language::isGerman());
    }

    void validatorMessages()
    {
        JobOptions job = JobOptions::defaults("test");
        QCOMPARE(GuiText::message(JobValidator::validate(job)),
                 QStringLiteral("Bitte wählen Sie einen Quellordner."));

        job.source = m_root.path() + "/missing";
        job.destination = m_root.path() + "/dest";
        QCOMPARE(GuiText::message(JobValidator::validate(job)),
                 QStringLiteral("Der Quellordner existiert nicht: %1").arg(job.source));

        QVERIFY(QDir().mkpath(m_root.path() + "/src"));
        job.source = m_root.path() + "/src";
        job.destination = job.source + "/inner";
        QCOMPARE(GuiText::message(JobValidator::validate(job)),
                 QStringLiteral("Das Ziel darf weder der Quellordner sein noch darin liegen."));
    }

    void emptySourceSafetyStop()
    {
        const QString src = m_root.path() + "/empty-src";
        const QString dst = m_root.path() + "/full-dst";
        QVERIFY(QDir().mkpath(src));
        QVERIFY(testutil::writeFile(dst + "/precious.mp3", "keep me"));
        JobOptions job = JobOptions::defaults("test");
        job.source = src;
        job.destination = dst;
        job.mode = SyncMode::Mirror;
        SyncControl control;
        const RunReport report = SyncEngine(job, control, {}).run();
        QVERIFY(report.status == RunStatus::Failed);
        QVERIFY(GuiText::message(report.fatalError).startsWith(QStringLiteral("Der Quellordner ist leer")));
    }

    void sideAndRepeatedPlaceholder()
    {
        QCOMPARE(GuiText::message(QStringLiteral("Destination: the disk BACKUP is not connected.")),
                 QStringLiteral("Ziel: Der Datenträger BACKUP ist nicht angeschlossen."));
        QCOMPARE(GuiText::message(QStringLiteral(
                     "Source: STICK (F:) is a different disk, not MUSIC. Connect MUSIC, or choose the "
                     "folder again in Oborcopy to use this disk.")),
                 QStringLiteral("Quelle: STICK (F:) ist ein anderer Datenträger, nicht MUSIC. Schließen "
                                "Sie MUSIC an oder wählen Sie den Ordner in Oborcopy neu, um diesen "
                                "Datenträger zu verwenden."));
    }

    void elsewhereHint()
    {
        const QString hint = QStringLiteral(" The folder exists on STICK (F:) as F:\\Music – open the job "
                                            "in Oborcopy to use it.");
        const QString germanHint = QStringLiteral(" Der Ordner existiert auf STICK (F:) als F:\\Music – öffnen "
                                                  "Sie den Auftrag in Oborcopy, um ihn zu verwenden.");
        // The engine adds a full stop to a message that has none.
        QCOMPARE(GuiText::message(QStringLiteral("The source folder does not exist: D:/Music.") + hint),
                 QStringLiteral("Der Quellordner existiert nicht: D:/Music.") + germanHint);
        QCOMPARE(GuiText::message(QStringLiteral("Cannot create the destination folder E:/Back.up.") + hint),
                 QStringLiteral("Der Zielordner E:/Back.up kann nicht erstellt werden.") + germanHint);
    }

    void pathsAndUnknownTextStayAsTheyAre()
    {
        // A folder called "Copy" is a path here, not the operation.
        QCOMPARE(GuiText::message(QStringLiteral("Cannot create folder Copy")),
                 QStringLiteral("Ordner Copy kann nicht erstellt werden"));
        QCOMPARE(GuiText::message(QStringLiteral("Write error: There is not enough space on the disk.")),
                 QStringLiteral("Schreibfehler: There is not enough space on the disk."));
        QCOMPARE(GuiText::message(QStringLiteral("Something the GUI does not know")),
                 QStringLiteral("Something the GUI does not know"));
        QCOMPARE(GuiText::message(QString()), QString());
    }

    void labels()
    {
        QCOMPARE(GuiText::status(RunStatus::CompletedWithErrors), QStringLiteral("Abgeschlossen mit Fehlern"));
        QCOMPARE(GuiText::message(QStringLiteral("Failed to start")), QStringLiteral("Start fehlgeschlagen"));
        QCOMPARE(GuiText::mode(SyncMode::Mirror), QStringLiteral("Spiegeln"));
        ScheduleEntry e;
        e.frequency = Frequency::Monthly;
        e.dayOfMonth = 0;
        e.time = QTime(2, 0);
        QCOMPARE(GuiText::describe(e), QStringLiteral("Monatlich am letzten Tag um 02:00"));
    }
};

QTEST_GUILESS_MAIN(TestGuiText)
#include "tst_guitext.moc"
