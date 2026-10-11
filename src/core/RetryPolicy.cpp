#include "RetryPolicy.h"

#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

namespace obor {

namespace {
constexpr double kJitter = 0.2;
}

RetryPolicy::RetryPolicy(const RetryOptions &options)
    : m_options(options)
{
    m_options.retries = std::max(0, m_options.retries);
    m_options.baseWaitMs = std::max(0, m_options.baseWaitMs);
    m_options.maxWaitMs = std::max(m_options.baseWaitMs, m_options.maxWaitMs);
}

int RetryPolicy::delayMs(int attempt) const
{
    const double factor = QRandomGenerator::global()->generateDouble() * 2.0 - 1.0;
    return delayMs(attempt, factor);
}

int RetryPolicy::delayMs(int attempt, double jitterFactor) const
{
    const int exponent = std::clamp(attempt - 1, 0, 30);
    const double raw = double(m_options.baseWaitMs) * std::pow(2.0, exponent);
    const double capped = std::min(raw, double(m_options.maxWaitMs));
    const double jitter = 1.0 + kJitter * std::clamp(jitterFactor, -1.0, 1.0);
    return int(std::lround(capped * jitter));
}

} // namespace obor
