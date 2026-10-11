#include "PathFilter.h"
#include "TestUtil.h"

#include <QTest>

using namespace obor;

class TestPathFilter : public QObject
{
    Q_OBJECT

private:
    static qint64 now() { return testutil::baseTimeMs(); }
    static QDateTime nowDt() { return QDateTime::fromMSecsSinceEpoch(now(), QTimeZone::utc()); }

private slots:
    void acceptsEverythingByDefault()
    {
        const PathFilter filter(FilterOptions{}, false, nowDt());
        QVERIFY(filter.acceptFileData("song.mp3", "a/song.mp3", 100, now()));
        QVERIFY(filter.acceptDirName("Album", "Artist/Album"));
    }

    void excludeFilePatterns()
    {
        FilterOptions o;
        o.excludeFiles = {"*.tmp", "Thumbs.db"};
        const PathFilter filter(o, true, nowDt());
        QVERIFY(!filter.acceptFileData("x.tmp", "a/x.tmp", 1, now()));
        QVERIFY(!filter.acceptFileData("thumbs.DB", "a/thumbs.DB", 1, now()));
        QVERIFY(filter.acceptFileData("x.flac", "a/x.flac", 1, now()));
    }

    void caseSensitivityFollowsVolume()
    {
        FilterOptions o;
        o.excludeFiles = {"*.TMP"};
        QVERIFY(filter(o, false).acceptFileData("x.tmp", "x.tmp", 1, now()));
        QVERIFY(!filter(o, true).acceptFileData("x.tmp", "x.tmp", 1, now()));
    }

    void includePatterns()
    {
        FilterOptions o;
        o.includeFiles = {"*.mp3", "*.flac"};
        const PathFilter filter(o, false, nowDt());
        QVERIFY(filter.acceptFileData("a.mp3", "a.mp3", 1, now()));
        QVERIFY(!filter.acceptFileData("a.jpg", "a.jpg", 1, now()));
    }

    void excludeDirsByNameAndPath()
    {
        FilterOptions o;
        o.excludeDirs = {"$RECYCLE.BIN", "Music/Temp", ".Trash-*"};
        const PathFilter filter(o, true, nowDt());
        QVERIFY(!filter.acceptDirName("$RECYCLE.BIN", "$RECYCLE.BIN"));
        QVERIFY(!filter.acceptDirName("Temp", "Music/Temp"));
        QVERIFY(filter.acceptDirName("Temp", "Other/Temp"));
        QVERIFY(!filter.acceptDirName(".Trash-1000", ".Trash-1000"));
    }

    void sizeLimits()
    {
        FilterOptions o;
        o.minSize = 10;
        o.maxSize = 100;
        const PathFilter filter(o, false, nowDt());
        QVERIFY(!filter.acceptFileData("a", "a", 9, now()));
        QVERIFY(filter.acceptFileData("a", "a", 10, now()));
        QVERIFY(filter.acceptFileData("a", "a", 100, now()));
        QVERIFY(!filter.acceptFileData("a", "a", 101, now()));
    }

    void ageLimits()
    {
        const qint64 day = 24LL * 3600 * 1000;
        FilterOptions o;
        o.minAgeDays = 2;  // skip files newer than 2 days
        o.maxAgeDays = 10; // skip files older than 10 days
        const PathFilter filter(o, false, nowDt());
        QVERIFY(!filter.acceptFileData("a", "a", 1, now() - day));
        QVERIFY(filter.acceptFileData("a", "a", 1, now() - 5 * day));
        QVERIFY(!filter.acceptFileData("a", "a", 1, now() - 11 * day));
    }

private:
    static PathFilter filter(const FilterOptions &o, bool ci) { return PathFilter(o, ci, nowDt()); }
};

QTEST_GUILESS_MAIN(TestPathFilter)
#include "tst_pathfilter.moc"
