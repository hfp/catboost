#include "pairwise_scoring.h"
#include "pairwise_leaves_calculation.h"

#include <util/generic/xrange.h>
#include <util/system/yassert.h>

#include <emmintrin.h>

void TPairwiseStats::Add(const TPairwiseStats& rhs) {
    Y_ASSERT(DerSums.size() == rhs.DerSums.size());

    for (auto leafIdx : xrange(DerSums.size())) {
        auto& dst = DerSums[leafIdx];
        const auto& add = rhs.DerSums[leafIdx];

        Y_ASSERT(dst.size() == add.size());

        for (auto bucketIdx : xrange(dst.size())) {
            dst[bucketIdx] += add[bucketIdx];
        }
    }

    Y_ASSERT(PairWeightStatistics.GetXSize() == rhs.PairWeightStatistics.GetXSize());
    Y_ASSERT(PairWeightStatistics.GetYSize() == rhs.PairWeightStatistics.GetYSize());

    for (auto leafIdx1 : xrange(PairWeightStatistics.GetYSize())) {
        auto dst1 = PairWeightStatistics[leafIdx1];
        const auto add1 = rhs.PairWeightStatistics[leafIdx1];

        for (auto leafIdx2 : xrange(PairWeightStatistics.GetXSize())) {
            auto& dst2 = dst1[leafIdx2];
            const auto& add2 = add1[leafIdx2];

            Y_ASSERT(dst2.size() == add2.size());

            for (auto bucketIdx : xrange(dst2.size())) {
                dst2[bucketIdx].Add(add2[bucketIdx]);
            }
        }
    }
}


static inline double XmmHorizontalAdd(__m128d x) {
    return _mm_cvtsd_f64(_mm_add_pd(x, _mm_shuffle_pd(x, x, /*swap halves*/ 0x1)));
}

static inline __m128d XmmFusedMultiplyAdd(const double* x, const double* y, __m128d z) {
    return _mm_add_pd(_mm_mul_pd(_mm_loadu_pd(x), _mm_loadu_pd(y)), z);
}

static inline __m128d XmmGather(const double* first, const double* second) {
    return _mm_loadh_pd(_mm_loadl_pd(_mm_undefined_pd(), first), second);
}

static double CalculateScore(const TVector<double>& avrg, const TVector<double>& sumDer, const TArray2D<double>& sumWeights) {
    const ui32 sumDerSize = sumDer.ysize();
    double score = 0;
    for (ui32 x = 0; x < sumDerSize; ++x) {
        const double* avrgData = avrg.data();
        const double* sumWeightsData = &sumWeights[x][0];
        __m128d subScore0 = _mm_setzero_pd();
        __m128d subScore2 = _mm_setzero_pd();
        for (ui32 y = 0; y + 4 <= sumDerSize; y += 4) {
            subScore0 = XmmFusedMultiplyAdd(avrgData + y + 0, sumWeightsData + y + 0, subScore0);
            subScore2 = XmmFusedMultiplyAdd(avrgData + y + 2, sumWeightsData + y + 2, subScore2);
        }
        double subScore = XmmHorizontalAdd(subScore0) + XmmHorizontalAdd(subScore2);
        for (ui32 y = sumDerSize & ~3u; y < sumDerSize; ++y) {
            subScore += avrgData[y] * sumWeightsData[y];
        }
        score += avrg[x] * (sumDer[x] - 0.5 * subScore);
    }
    return score;
}

void CalculatePairwiseScore(
    const TPairwiseStats& pairwiseStats,
    int bucketCount,
    ESplitType splitType,
    float l2DiagReg,
    float pairwiseBucketWeightPriorReg,
    TVector<TScoreBin>* scoreBins
) {
    scoreBins->yresize(bucketCount);
    scoreBins->back() = TScoreBin();

    const auto& derSums = pairwiseStats.DerSums;
    const auto& pairWeightStatistics = pairwiseStats.PairWeightStatistics;

    const int leafCount = derSums.ysize();
    TVector<double> derSum(2 * leafCount, 0.0);
    TArray2D<double> weightSum(2 * leafCount, 2 * leafCount);
    weightSum.FillZero();
    Y_ASSERT(splitType == ESplitType::OnlineCtr || splitType == ESplitType::FloatFeature);

    for (int leafId = 0; leafId < leafCount; ++leafId) {
        double& derDst = derSum[2 * leafId + 1];
        const auto& derSrc = derSums[leafId];
        for (int bucketId = 0; bucketId < bucketCount; ++bucketId) {
            derDst += derSrc[bucketId];
        }
    }

    for (int y = 0; y < leafCount; ++y) {
        const auto& baseData = pairWeightStatistics[y];
        auto& weightSy1 = weightSum[2 * y + 1];
        auto& weightS11 = weightSy1[2 * y + 1];
        for (int x = y + 1; x < leafCount; ++x) {
            const TBucketPairWeightStatistics* xyData = pairWeightStatistics[x][y].data();
            const TBucketPairWeightStatistics* yxData = baseData[x].data();
            __m128d totalXY0 = _mm_setzero_pd();
            __m128d totalXY2 = _mm_setzero_pd();
            __m128d totalYX0 = _mm_setzero_pd();
            __m128d totalYX2 = _mm_setzero_pd();
            for (int bucketId = 0; bucketId + 4 <= bucketCount; bucketId += 4) {
                totalXY0 = _mm_add_pd(totalXY0, XmmGather(&xyData[bucketId + 0].SmallerBorderWeightSum, &xyData[bucketId + 1].SmallerBorderWeightSum));
                totalXY2 = _mm_add_pd(totalXY2, XmmGather(&xyData[bucketId + 2].SmallerBorderWeightSum, &xyData[bucketId + 3].SmallerBorderWeightSum));
                totalYX0 = _mm_add_pd(totalYX0, XmmGather(&yxData[bucketId + 0].SmallerBorderWeightSum, &yxData[bucketId + 1].SmallerBorderWeightSum));
                totalYX2 = _mm_add_pd(totalYX2, XmmGather(&yxData[bucketId + 2].SmallerBorderWeightSum, &yxData[bucketId + 3].SmallerBorderWeightSum));
            }
            double total = XmmHorizontalAdd(totalXY0) + XmmHorizontalAdd(totalXY2) + XmmHorizontalAdd(totalYX0) + XmmHorizontalAdd(totalYX2);
            for (int bucketId = bucketCount & ~3u; bucketId < bucketCount; ++bucketId) {
                total += xyData[bucketId].SmallerBorderWeightSum + yxData[bucketId].SmallerBorderWeightSum;
            }
            weightSy1[2 * x + 1] += total;
            weightSum[2 * x + 1][2 * y + 1] += total;
            weightSum[2 * x + 1][2 * x + 1] -= total;
            weightS11 -= total;
        }
    }

    for (int splitId = 0; splitId < bucketCount - 1; ++splitId) {
        for (int y = 0; y < leafCount; ++y) {
            const double derDelta = derSums[y][splitId];
            derSum[2 * y] += derDelta;
            derSum[2 * y + 1] -= derDelta;

            const auto& pairWeightStatistics_Y0 = pairWeightStatistics[y];
            const double weightDelta = (pairWeightStatistics_Y0[y][splitId].SmallerBorderWeightSum -
                pairWeightStatistics_Y0[y][splitId].GreaterBorderRightWeightSum);
            auto& weightSy0 = weightSum[2 * y];
            auto& weightSy1 = weightSum[2 * y + 1];
            auto& weightS00 = weightSy0[2 * y];
            auto& weightS11 = weightSy1[2 * y + 1];
            weightSy0[2 * y + 1] += weightDelta;
            weightSy1[2 * y] += weightDelta;
            weightS00 -= weightDelta;
            weightS11 -= weightDelta;

            for (int x = y + 1; x < leafCount; ++x) {
                const TBucketPairWeightStatistics& xy = pairWeightStatistics[x][y][splitId];
                const TBucketPairWeightStatistics& yx = pairWeightStatistics_Y0[x][splitId];

                const double w00Delta = xy.GreaterBorderRightWeightSum + yx.GreaterBorderRightWeightSum;
                const double w01Delta = xy.SmallerBorderWeightSum - xy.GreaterBorderRightWeightSum;
                const double w10Delta = yx.SmallerBorderWeightSum - yx.GreaterBorderRightWeightSum;
                const double w11Delta = -(xy.SmallerBorderWeightSum + yx.SmallerBorderWeightSum);

                weightSum[2 * x][2 * y] += w00Delta;
                weightSy0[2 * x] += w00Delta;
                weightSum[2 * x][2 * y + 1] += w01Delta;
                weightSy1[2 * x] += w01Delta;
                weightSum[2 * x + 1][2 * y] += w10Delta;
                weightSy0[2 * x + 1] += w10Delta;
                weightSum[2 * x + 1][2 * y + 1] += w11Delta;
                weightSy1[2 * x + 1] += w11Delta;

                weightS00 -= w00Delta + w10Delta;
                weightSum[2 * x][2 * x] -= w00Delta + w01Delta;
                weightSum[2 * x + 1][2 * x + 1] -= w10Delta + w11Delta;
                weightS11 -= w01Delta + w11Delta;
            }
        }

        const TVector<double>& leafValues = CalculatePairwiseLeafValues(weightSum, derSum, l2DiagReg, pairwiseBucketWeightPriorReg);
        (*scoreBins)[splitId].D2 = 1.0;
        (*scoreBins)[splitId].DP = CalculateScore(leafValues, derSum, weightSum);
    }
}

