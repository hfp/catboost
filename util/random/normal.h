#pragma once

#include <cmath>

// sometimes we need stateless normal distribution...

/*
 * normal distribution with Box-Muller transform
 * http://www.design.caltech.edu/erik/Misc/Gaussian.html
 */
template <typename T, typename TRng>
static inline T StdNormalDistribution(TRng&& rng) noexcept {
    T x, r;

    do {
        x = static_cast<T>(rng.GenRandReal1()) * T(2) - T(1);
    } while (T(M_SQRT1_2) < x || T(0) == x);

    return std::sqrt(std::abs(T(M_LN2) + log(x * x)));
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
