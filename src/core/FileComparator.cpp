#include "FileComparator.h"

#include <cstdlib>

namespace obor {

namespace {
constexpr qint64 kHourMs = 60LL * 60 * 1000;
}

FileComparator::FileComparator(int toleranceMs, bool dstTolerance)
    : m_toleranceMs(qMax(0, toleranceMs))
    , m_dstTolerance(dstTolerance)
{
}

bool FileComparator::timesEqual(qint64 sourceMs, qint64 destMs) const
{
    const qint64 diff = std::llabs(sourceMs - destMs);
    if (diff <= m_toleranceMs)
        return true;
    // Daylight saving shifts on FAT volumes move times by exactly one hour.
    return m_dstTolerance && std::llabs(diff - kHourMs) <= m_toleranceMs;
}

Comparison FileComparator::compare(const FileEntry &source, const FileEntry &dest) const
{
    Comparison result;
    result.sameSize = source.size == dest.size;
    if (!timesEqual(source.mtimeMs, dest.mtimeMs))
        result.timeOrder = source.mtimeMs > dest.mtimeMs ? 1 : -1;
    return result;
}

} // namespace obor
