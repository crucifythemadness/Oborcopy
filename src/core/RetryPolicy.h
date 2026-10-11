#pragma once

#include "JobOptions.h"

namespace obor {

// Exponential backoff: wait = min(base * 2^(attempt-1), max), +/- 20 % jitter.
// Jitter prevents several parallel copies from retrying in lock step.
class RetryPolicy
{
public:
    explicit RetryPolicy(const RetryOptions &options);

    int maxRetries() const { return m_options.retries; }
    // Delay before retry number 'attempt' (1-based) with random jitter.
    int delayMs(int attempt) const;
    // Deterministic variant; jitterFactor in [-1, 1] scales the +/-20 % band.
    int delayMs(int attempt, double jitterFactor) const;

private:
    RetryOptions m_options;
};

} // namespace obor
