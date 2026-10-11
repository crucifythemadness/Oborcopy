#pragma once

#include "JobOptions.h"

#include <QString>

namespace obor::JobValidator {

// Returns an empty string when the job can run, otherwise a user-facing reason.
QString validate(const JobOptions &job);

// True if 'inner' is the same folder as or lies inside 'outer'.
bool isSameOrInside(const QString &inner, const QString &outer);

} // namespace obor::JobValidator
