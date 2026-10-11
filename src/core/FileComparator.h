#pragma once

#include "FileEntry.h"

namespace obor {

struct Comparison
{
    bool sameSize = true;
    int timeOrder = 0; // -1 source older, 0 equal (within tolerance), +1 source newer
    bool identical() const { return sameSize && timeOrder == 0; }
};

// Compares file metadata. Timestamps within the tolerance count as equal,
// which avoids re-copying everything on FAT/exFAT (2-second resolution).
class FileComparator
{
public:
    FileComparator(int toleranceMs, bool dstTolerance);

    bool timesEqual(qint64 sourceMs, qint64 destMs) const;
    Comparison compare(const FileEntry &source, const FileEntry &dest) const;
    int toleranceMs() const { return m_toleranceMs; }

private:
    int m_toleranceMs;
    bool m_dstTolerance;
};

} // namespace obor
