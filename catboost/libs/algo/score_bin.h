#pragma once

#include <util/generic/vector.h>
#include <util/generic/xrange.h>

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
#if defined(SCORE_BIN_TLS)
    Y_STATIC_THREAD(TVector<double>) scoresLocal; // TVector is non-POD
    TVector<double>& scores = TlsRef(scoresLocal);
    scores.resize(scoreBin.size());
#else
    TVector<double> scores(scoreBin.size());
#endif
    for (auto i : xrange(scoreBin.size())) {
        scores[i] = scoreBin[i].GetScore();
    }
    return scores;
}
