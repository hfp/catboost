#pragma once

#include <util/generic/vector.h>
#if !defined(SCORE_BIN_TLS)
# include <util/system/tls.h>
# define SCORE_BIN_TLS
#endif

#include <cmath>

// The class that stores final stats for a split and provides interface to calculate the deterministic score.
struct TScoreBin {
    double DP = 0, D2 = 1e-100;

    inline double GetScore() const {
        return DP / sqrt(D2);
    }
};

// Helper function that calculates deterministic scores given bins with statistics for each split.
inline
#if defined(SCORE_BIN_TLS)
const TVector<double>&
#else
TVector<double>
#endif
GetScores(const TVector<TScoreBin>& scoreBin) {
    const int splitCount = scoreBin.ysize() - 1;
#if defined(SCORE_BIN_TLS)
    Y_STATIC_THREAD(TVector<double>) scores_local; // TVector is non-POD
    TVector<double>& scores = TlsRef(scores_local);
    scores.resize(splitCount);
#else
    TVector<double> scores(splitCount);
#endif
    for (int splitIdx = 0; splitIdx < splitCount; ++splitIdx) {
        scores[splitIdx] = scoreBin[splitIdx].GetScore();
    }
    return scores;
}
