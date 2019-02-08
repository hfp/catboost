#pragma once

#include <cmath>

#if !defined(NORMAL_LOG_FAST)
# include <library/fast_log/fast_log.h>
# define NORMAL_LOG_FAST 1
#endif

// sometimes we need stateless normal distribution...

/*
 * normal distribution with Box-Muller transform
 * http://www.design.caltech.edu/erik/Misc/Gaussian.html
 */
template <typename T, typename TRng>
static inline T StdNormalDistribution(TRng&& rng) noexcept {
    T x;

    do {
        x = static_cast<T>(rng.GenRandReal1()) * T(2) - T(1);
    } while (T(M_SQRT1_2) < x || T(0) == x);
#if defined(NORMAL_LOG_FAST)
# if (3 <= NORMAL_LOG_FAST)
    return std::sqrt(std::abs(T(M_LN2) + FastestLogf(x * x)));
# elif (2 <= NORMAL_LOG_FAST)
    return std::sqrt(std::abs(T(M_LN2) + FasterLogf(x * x)));
# else
    return std::sqrt(std::abs(T(M_LN2) + FastLogf(x * x)));
# endif
#else
    return std::sqrt(std::abs(T(M_LN2) + std::log(x * x)));
#endif
}

template <typename T, typename TRng>
static inline T NormalDistribution(TRng&& rng, T m, T d) noexcept {
    return StdNormalDistribution<T>(rng) * d + m;
}

// specialized for float, double, long double
template <class T>
T StdNormalRandom() noexcept;

template <class T>
static inline T NormalRandom(T m, T d) noexcept {
    return StdNormalRandom<T>() * d + m;
}
