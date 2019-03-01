#include "score_calcer.h"

#include "index_calcer.h"
#include "online_predictor.h"

#include <catboost/libs/data_types/pair.h>
#include <catboost/libs/index_range/index_range.h>
#include <catboost/libs/helpers/map_merge.h>
#include <catboost/libs/options/defaults_helper.h>

#include <util/generic/array_ref.h>

#include <type_traits>

#if !defined(SCORE_CALCER_TLS) && defined(__TLS_OPTS)
# include <util/system/tls.h>
# define SCORE_CALCER_TLS
#endif

using namespace NCB;


namespace {

    // Statistics (sums for score calculation) are stored in an array.
    // This class helps navigating in this array.
    struct TStatsIndexer {
    public:
        const int BucketCount;

    public:
        explicit TStatsIndexer(int bucketCount)
            : BucketCount(bucketCount) {
        }

        int CalcSize(int depth) const {
            return (1U << depth) * BucketCount;
        }

        int GetIndex(int leafIndex, int bucketIndex) const {
            return BucketCount * leafIndex + bucketIndex;
        }
    };


    template <class T>
    class TDataRefOptionalHolder {
    public:
        TDataRefOptionalHolder() = default;

        // Buf not used, init from external data
        explicit TDataRefOptionalHolder(TArrayRef<T> extData)
            : Data(extData)
        {}

        // noninitializing
        explicit TDataRefOptionalHolder(size_t size)
        {
            Buf.yresize(size);
            Data = TArrayRef<T>(Buf);
        }

        bool NonInited() const {
            return Data.Data() == nullptr;
        }

        TArrayRef<T> GetData() {
            return Data;
        }

        TConstArrayRef<T> GetData() const {
            return Data;
        }

    private:
        TArrayRef<T> Data;
        TVector<T> Buf;
    };

    using TBucketStatsRefOptionalHolder = TDataRefOptionalHolder<TBucketStats>;
}


/* A helper function that returns calculated ctr values for this projection
   (== feature or feature combination) from cache.
*/
inline static const TOnlineCTR& GetCtr(
    const std::tuple<const TOnlineCTRHash&,
    const TOnlineCTRHash&>& allCtrs,
    const TProjection& proj
) {
    static const constexpr size_t OnlineSingleCtrsIndex = 0;
    static const constexpr size_t OnlineCTRIndex = 1;
    return proj.HasSingleFeature() ?
              std::get<OnlineSingleCtrsIndex>(allCtrs).at(proj)
            : std::get<OnlineCTRIndex>(allCtrs).at(proj);
}


// Helper function for calculating index of leaf for each document given a new split.
// Calculates indices when a permutation is given.
template <typename TBucketIndexType, typename TFullIndexType>
inline static void SetSingleIndex(
    const TCalcScoreFold& fold,
    const TStatsIndexer& indexer,
    TBucketIndexType* bucketIndex,
    const ui32* bucketIndexing, // can be nullptr for simple case, use bucketBeginOffset instead then
    const int bucketBeginOffset,
    const int permBlockSize,
    NCB::TIndexRange<int> docIndexRange, // aligned by permutation blocks in docPermutation
    TVector<TFullIndexType>* singleIdx // already of proper size
) {
    const int docCount = fold.GetDocCount();
    const TIndexType* indices = GetDataPtr(fold.Indices);

    if (bucketIndexing == nullptr) {
        for (int doc : docIndexRange.Iter()) {
            (*singleIdx)[doc] = indexer.GetIndex(indices[doc], bucketIndex[bucketBeginOffset + doc]);
        }
    } else if (permBlockSize > 1) {
        const int blockCount = (docCount + permBlockSize - 1) / permBlockSize;
        Y_ASSERT(   (static_cast<int>(bucketIndexing[0]) / permBlockSize + 1 == blockCount)
                 || (static_cast<int>(bucketIndexing[0]) + permBlockSize - 1
                     == static_cast<int>(bucketIndexing[permBlockSize - 1])));
        int blockStart = docIndexRange.Begin;
        while (blockStart < docIndexRange.End) {
            const int blockIdx = static_cast<int>(bucketIndexing[blockStart]) / permBlockSize;
            const int nextBlockStart = blockStart + (
                blockIdx + 1 == blockCount ? docCount - blockIdx * permBlockSize : permBlockSize
            );
            const int originalBlockIdx = static_cast<int>(bucketIndexing[blockStart]);
            for (int doc = blockStart; doc < nextBlockStart; ++doc) {
                const int originalDocIdx = originalBlockIdx + doc - blockStart;
                (*singleIdx)[doc] = indexer.GetIndex(indices[doc], bucketIndex[originalDocIdx]);
            }
            blockStart = nextBlockStart;
        }
    } else {
        for (int doc : docIndexRange.Iter()) {
            const ui32 originalDocIdx = bucketIndexing[doc];
            (*singleIdx)[doc] = indexer.GetIndex(indices[doc], bucketIndex[originalDocIdx]);
        }
    }
}


// Calculate index of leaf for each document given a new split ensemble.
template <typename TFullIndexType>
inline static void BuildSingleIndex(
    const TCalcScoreFold& fold,
    const TQuantizedForCPUObjectsDataProvider& objectsDataProvider,
    const std::tuple<const TOnlineCTRHash&, const TOnlineCTRHash&>& allCtrs,
    const TSplitEnsemble& splitEnsemble,
    const TStatsIndexer& indexer,
    NCB::TIndexRange<int> docIndexRange,
    TVector<TFullIndexType>* singleIdx // already of proper size
) {
    if (splitEnsemble.IsSplitOfType(ESplitType::OnlineCtr)) {
        const TCtr& ctr = splitEnsemble.SplitCandidate.Ctr;
        const bool simpleIndexing = fold.CtrDataPermutationBlockSize == fold.GetDocCount();
        const ui32* docInFoldIndexing = simpleIndexing ? nullptr : GetDataPtr(fold.IndexInFold);
        SetSingleIndex(
            fold,
            indexer,
            GetCtr(allCtrs, ctr.Projection).Feature[ctr.CtrIdx][ctr.TargetBorderIdx][ctr.PriorIdx].data(),
            docInFoldIndexing,
            0,
            fold.CtrDataPermutationBlockSize,
            docIndexRange,
            singleIdx
        );
    } else {
        const bool simpleIndexing = fold.NonCtrDataPermutationBlockSize == fold.GetDocCount();
        const ui32* docInDataProviderIndexing =
            simpleIndexing ?
            nullptr
            : fold.LearnPermutationFeaturesSubset.Get<TIndexedSubset<ui32>>().data();
        const int docInDataProviderBeginOffset = simpleIndexing ? fold.FeaturesSubsetBegin : 0;

        if (splitEnsemble.IsBinarySplitsPack) {
            SetSingleIndex(
                fold,
                indexer,
                (**objectsDataProvider.GetBinaryFeaturesPack(splitEnsemble.BinarySplitsPack.PackIdx).GetSrc())
                    .Data(),
                docInDataProviderIndexing,
                docInDataProviderBeginOffset,
                fold.NonCtrDataPermutationBlockSize,
                docIndexRange,
                singleIdx
            );
        } else {
            const auto& splitCandidate = splitEnsemble.SplitCandidate;

            if (splitCandidate.Type == ESplitType::FloatFeature) {
                SetSingleIndex(
                    fold,
                    indexer,
                    *((*objectsDataProvider.GetNonPackedFloatFeature((ui32)splitCandidate.FeatureIdx))
                        ->GetArrayData().GetSrc()),
                    docInDataProviderIndexing,
                    docInDataProviderBeginOffset,
                    fold.NonCtrDataPermutationBlockSize,
                    docIndexRange,
                    singleIdx
                );
            } else {
                Y_ASSERT(splitCandidate.Type == ESplitType::OneHotFeature);
                SetSingleIndex(
                    fold,
                    indexer,
                    *((*objectsDataProvider.GetNonPackedCatFeature((ui32)splitCandidate.FeatureIdx))
                        ->GetArrayData().GetSrc()),
                    docInDataProviderIndexing,
                    docInDataProviderBeginOffset,
                    fold.NonCtrDataPermutationBlockSize,
                    docIndexRange,
                    singleIdx
                );
            }
        }
    }
}


// Update bootstraped sums on docIndexRange in a bucket
template <typename TFullIndexType>
inline static void UpdateWeighted(
    const TVector<TFullIndexType>& singleIdx,
    const double* weightedDer,
    const float* sampleWeights,
    NCB::TIndexRange<int> docIndexRange,
    TBucketStats* stats
) {
    for (int doc : docIndexRange.Iter()) {
        TBucketStats& leafStats = stats[singleIdx[doc]];
        leafStats.SumWeightedDelta += weightedDer[doc];
        leafStats.SumWeight += sampleWeights[doc];
    }
}


// Update not bootstraped sums on docIndexRange in a bucket
template <typename TFullIndexType>
inline static void UpdateDeltaCount(
    const TVector<TFullIndexType>& singleIdx,
    const double* derivatives,
    const float* learnWeights,
    NCB::TIndexRange<int> docIndexRange,
    TBucketStats* stats
) {
    if (learnWeights == nullptr) {
        for (int doc : docIndexRange.Iter()) {
            TBucketStats& leafStats = stats[singleIdx[doc]];
            leafStats.SumDelta += derivatives[doc];
            leafStats.Count += 1;
        }
    } else {
        for (int doc : docIndexRange.Iter()) {
            TBucketStats& leafStats = stats[singleIdx[doc]];
            leafStats.SumDelta += derivatives[doc];
            leafStats.Count += learnWeights[doc];
        }
    }
}


template <typename TFullIndexType>
inline static void CalcStatsKernel(
    bool isCaching,
    const TVector<TFullIndexType>& singleIdx,
    const TCalcScoreFold& fold,
    bool isPlainMode,
    const TStatsIndexer& indexer,
    int depth,
    const TCalcScoreFold::TBodyTail& bt,
    int dim,
    NCB::TIndexRange<int> docIndexRange,
    TBucketStats* stats
) {
    Y_ASSERT(!isCaching || depth > 0);
    if (isCaching) {
        Fill(
            stats + indexer.CalcSize(depth - 1),
            stats + indexer.CalcSize(depth),
            TBucketStats{0, 0, 0, 0}
        );
    } else {
        Fill(stats, stats + indexer.CalcSize(depth), TBucketStats{0, 0, 0, 0});
    }

    if (bt.TailFinish > docIndexRange.Begin) {
        const bool hasPairwiseWeights = !bt.PairwiseWeights.empty();
        const float* weightsData = hasPairwiseWeights ?
            GetDataPtr(bt.PairwiseWeights) : GetDataPtr(fold.LearnWeights);
        const float* sampleWeightsData = hasPairwiseWeights ?
            GetDataPtr(bt.SamplePairwiseWeights) : GetDataPtr(fold.SampleWeights);

        int tailFinishInRange = Min((int)bt.TailFinish, docIndexRange.End);

        if (isPlainMode) {
            UpdateWeighted(
                singleIdx,
                GetDataPtr(bt.SampleWeightedDerivatives[dim]),
                sampleWeightsData,
                NCB::TIndexRange<int>(docIndexRange.Begin, tailFinishInRange),
                stats
            );
        } else {
            if (bt.BodyFinish > docIndexRange.Begin) {
                UpdateDeltaCount(
                    singleIdx,
                    GetDataPtr(bt.WeightedDerivatives[dim]),
                    weightsData,
                    NCB::TIndexRange<int>(docIndexRange.Begin, Min((int)bt.BodyFinish, docIndexRange.End)),
                    stats
                );
            }
            if (tailFinishInRange > bt.BodyFinish) {
                UpdateWeighted(
                    singleIdx,
                    GetDataPtr(bt.SampleWeightedDerivatives[dim]),
                    sampleWeightsData,
                    NCB::TIndexRange<int>(Max((int)bt.BodyFinish, docIndexRange.Begin), tailFinishInRange),
                    stats
                );
            }
        }
    }
}

inline static void FixUpStats(
    int depth,
    const TStatsIndexer& indexer,
    bool selectedSplitValue,
    TBucketStats* stats
) {
    const int halfOfStats = indexer.CalcSize(depth - 1);
    if (selectedSplitValue == true) {
        for (int statIdx = 0; statIdx < halfOfStats; ++statIdx) {
            stats[statIdx].Remove(stats[statIdx + halfOfStats]);
        }
    } else {
        for (int statIdx = 0; statIdx < halfOfStats; ++statIdx) {
            stats[statIdx].Remove(stats[statIdx + halfOfStats]);
            DoSwap(stats[statIdx], stats[statIdx + halfOfStats]);
        }
    }
}


template <typename TFullIndexType, typename TIsCaching>
static void CalcStatsImpl(
    const TCalcScoreFold& fold,
    const TQuantizedForCPUObjectsDataProvider& objectsDataProvider,
    const TFlatPairsInfo& pairs,
    const std::tuple<const TOnlineCTRHash&, const TOnlineCTRHash&>& allCtrs,
    const TSplitEnsemble& splitEnsemble,
    const TStatsIndexer& indexer,
    const TIsCaching& /*isCaching*/,
    bool /*isPlainMode*/,
    int depth,
    int /*splitStatsCount*/,
    NPar::TLocalExecutor* localExecutor,
    TPairwiseStats* stats
) {
    const int approxDimension = fold.GetApproxDimension();
    const int leafCount = 1 << depth;

    Y_ASSERT(approxDimension == 1 && fold.GetBodyTailCount() == 1);

    const int docCount = fold.GetDocCount();
    auto weightedDerivativesData = MakeArrayRef(
        fold.BodyTailArr[0].WeightedDerivatives[0].data(),
        docCount
    );
    const auto blockCount = fold.GetCalcStatsIndexRanges().RangesCount();
    const auto docPart = CeilDiv(docCount, blockCount);

    const auto pairCount = pairs.ysize();
    const auto pairPart = CeilDiv(pairCount, blockCount);

    NCB::MapMerge(
        localExecutor,
        fold.GetCalcStatsIndexRanges(),
        /*mapFunc*/[&](NCB::TIndexRange<int> partIndexRange, TPairwiseStats* output) {
            Y_ASSERT(!partIndexRange.Empty());

            auto docIndexRange = NCB::TIndexRange<int>(
                Min(docCount, docPart * partIndexRange.Begin),
                Min(docCount, docPart * partIndexRange.End)
            );

            auto pairIndexRange = NCB::TIndexRange<int>(
                Min(pairCount, pairPart * partIndexRange.Begin),
                Min(pairCount, pairPart * partIndexRange.End)
            );

            if (splitEnsemble.IsBinarySplitsPack) {
                const TBinaryFeaturesPack* bucketSrcData =
                    (**objectsDataProvider.GetBinaryFeaturesPack(splitEnsemble.BinarySplitsPack.PackIdx)
                        .GetSrc()).Data();
                const ui32* bucketIndexing
                    = fold.LearnPermutationFeaturesSubset.Get<TIndexedSubset<ui32>>().data();

                output->DerSums = ComputeDerSums(
                    weightedDerivativesData,
                    leafCount,
                    indexer.BucketCount,
                    fold.Indices,
                    [bucketSrcData, bucketIndexing](ui32 docIdx) {
                        return bucketSrcData[bucketIndexing[docIdx]];
                    },
                    docIndexRange
                );
                auto pairWeightStatistics = ComputePairWeightStatisticsForBinaryFeaturesPacks(
                    pairs,
                    leafCount,
                    indexer.BucketCount,
                    fold.Indices,
                    [bucketSrcData, bucketIndexing](ui32 docIdx) {
                        return bucketSrcData[bucketIndexing[docIdx]];
                    },
                    pairIndexRange
                );
                output->PairWeightStatistics.Swap(pairWeightStatistics);

                output->SplitEnsembleSpec = TSplitEnsembleSpec::BinarySplitsPack();
            } else {
                const auto& splitCandidate = splitEnsemble.SplitCandidate;

                auto setOutput = [&] (auto&& getBucketFunc) {
                    output->DerSums = ComputeDerSums(
                        weightedDerivativesData,
                        leafCount,
                        indexer.BucketCount,
                        fold.Indices,
                        getBucketFunc,
                        docIndexRange
                    );
                    auto pairWeightStatistics = ComputePairWeightStatistics(
                        pairs,
                        leafCount,
                        indexer.BucketCount,
                        fold.Indices,
                        getBucketFunc,
                        pairIndexRange
                    );
                    output->PairWeightStatistics.Swap(pairWeightStatistics);
                    output->SplitEnsembleSpec = TSplitEnsembleSpec::OneSplit(splitCandidate.Type);
                };

                if (splitCandidate.Type == ESplitType::OnlineCtr) {
                    const TCtr& ctr = splitCandidate.Ctr;
                    TConstArrayRef<ui8> buckets =
                        GetCtr(allCtrs, ctr.Projection).Feature[ctr.CtrIdx][ctr.TargetBorderIdx][ctr.PriorIdx];
                    setOutput([buckets](ui32 docIdx) { return buckets[docIdx]; });
                } else if (splitCandidate.Type == ESplitType::FloatFeature) {
                    const ui8* bucketSrcData =
                        *((*objectsDataProvider.GetNonPackedFloatFeature((ui32)splitCandidate.FeatureIdx))
                            ->GetArrayData().GetSrc());
                    const ui32* bucketIndexing
                        = fold.LearnPermutationFeaturesSubset.Get<TIndexedSubset<ui32>>().data();
                    setOutput(
                        [bucketSrcData, bucketIndexing](ui32 docIdx) {
                            return bucketSrcData[bucketIndexing[docIdx]];
                        }
                    );
                } else {
                    Y_ASSERT(splitCandidate.Type == ESplitType::OneHotFeature);
                    const ui32* bucketSrcData =
                        *((*objectsDataProvider.GetNonPackedCatFeature((ui32)splitCandidate.FeatureIdx))
                            ->GetArrayData().GetSrc());
                    const ui32* bucketIndexing
                        = fold.LearnPermutationFeaturesSubset.Get<TIndexedSubset<ui32>>().data();
                    setOutput(
                        [bucketSrcData, bucketIndexing](ui32 docIdx) {
                            return bucketSrcData[bucketIndexing[docIdx]];
                        }
                    );
                }
            }
        },
        /*mergeFunc*/[&](TPairwiseStats* output, const TVector<TPairwiseStats>& addVector) {
            for (const auto& addItem : addVector) {
                output->Add(addItem);
            }
        },
        stats
    );
}


template <typename TFullIndexType, typename TIsCaching>
static void CalcStatsImpl(
    const TCalcScoreFold& fold,
    const TQuantizedForCPUObjectsDataProvider& objectsDataProvider,
    const TFlatPairsInfo& /*pairs*/,
    const std::tuple<const TOnlineCTRHash&, const TOnlineCTRHash&>& allCtrs,
    const TSplitEnsemble& splitEnsemble,
    const TStatsIndexer& indexer,
    const TIsCaching& isCaching,
    bool isPlainMode,
    int depth,
    int splitStatsCount,
    NPar::TLocalExecutor* localExecutor,
    TBucketStatsRefOptionalHolder* stats
) {
    Y_ASSERT(!isCaching || depth > 0);

    const int docCount = fold.GetDocCount();
#if defined(SCORE_CALCER_TLS)
    Y_STATIC_THREAD(TVector<TFullIndexType>) singleIdxLocal; // TVector is non-POD
    TVector<TFullIndexType>& singleIdx = TlsRef(singleIdxLocal);
    singleIdx.yresize(docCount);
#else
    TVector<TFullIndexType> singleIdx(docCount);
#endif
    const int bodyTailCount = fold.GetBodyTailCount(), approxDimension = fold.GetApproxDimension();
    const int statsCount = bodyTailCount * approxDimension * splitStatsCount;
    const int filledSplitStatsCount = indexer.CalcSize(depth);

    // bodyFunc must accept (bodyTailIdx, dim, bucketStatsArrayBegin) params
    auto forEachBodyTailAndApproxDimension = [&](auto bodyFunc) {
        for (int bodyTailIdx : xrange(bodyTailCount)) {
            for (int dim : xrange(approxDimension)) {
                bodyFunc(bodyTailIdx, dim, (bodyTailIdx * approxDimension + dim) * splitStatsCount);
            }
        }
    };

    NCB::MapMerge(
        localExecutor,
        fold.GetCalcStatsIndexRanges(),
        /*mapFunc*/[&](NCB::TIndexRange<int> indexRange, TBucketStatsRefOptionalHolder* output) {
            NCB::TIndexRange<int> docIndexRange = fold.HasQueryInfo() ?
                NCB::TIndexRange<int>(
                    fold.LearnQueriesInfo[indexRange.Begin].Begin,
                    (indexRange.End == 0) ? 0 : fold.LearnQueriesInfo[indexRange.End - 1].End
                )
                : indexRange;

            BuildSingleIndex(
                fold,
                objectsDataProvider,
                allCtrs,
                splitEnsemble,
                indexer,
                docIndexRange,
                &singleIdx);

            if (output->NonInited()) {
                (*output) = TBucketStatsRefOptionalHolder(statsCount);
            } else {
                Y_ASSERT(docIndexRange.Begin == 0);
            }

            forEachBodyTailAndApproxDimension(
                [&](int bodyTailIdx, int dim, int bucketStatsArrayBegin) {
                    TBucketStats* statsSubset = output->GetData().Data() + bucketStatsArrayBegin;
                    CalcStatsKernel(
                        isCaching && (indexRange.Begin == 0),
                        singleIdx,
                        fold,
                        isPlainMode,
                        indexer,
                        depth,
                        fold.BodyTailArr[bodyTailIdx],
                        dim,
                        docIndexRange,
                        statsSubset
                    );
                }
            );
        },
        /*mergeFunc*/[&](
            TBucketStatsRefOptionalHolder* output,
            const TVector<TBucketStatsRefOptionalHolder>& addVector
        ) {
            forEachBodyTailAndApproxDimension(
                [&](int /*bodyTailIdx*/, int /*dim*/, int bucketStatsArrayBegin) {
                    TBucketStats* outputStatsSubset =
                        output->GetData().Data() + bucketStatsArrayBegin;

                    for (const auto& addItem : addVector) {
                        const TBucketStats* addStatsSubset =
                            addItem.GetData().Data() + bucketStatsArrayBegin;
                        for (size_t i : xrange(filledSplitStatsCount)) {
                            (outputStatsSubset + i)->Add(*(addStatsSubset + i));
                        }
                    }
                }
            );
        },
        stats
    );

    if (isCaching) {
        forEachBodyTailAndApproxDimension(
            [&](int /*bodyTailIdx*/, int /*dim*/, int bucketStatsArrayBegin) {
                TBucketStats* statsSubset = stats->GetData().Data() + bucketStatsArrayBegin;
                FixUpStats(depth, indexer, fold.SmallestSplitSideValue, statsSubset);
            }
        );
    }
}


// Calculate score numerator summand
inline static double CountDp(double avrg, const TBucketStats& leafStats) {
    return avrg * leafStats.SumWeightedDelta;
}

// Calculate score denominator summand
inline static double CountD2(double avrg, const TBucketStats& leafStats) {
    return avrg * avrg * leafStats.SumWeight;
}


inline void UpdateScoreBin(
    bool isPlainMode,
    float l2Regularizer,
    double sumAllWeights,
    int allDocCount,
    const TBucketStats& trueStats,
    const TBucketStats& falseStats,
    TScoreBin* scoreBin
) {
    double trueAvrg, falseAvrg;
    if (isPlainMode) {
        trueAvrg = CalcAverage(
            trueStats.SumWeightedDelta,
            trueStats.SumWeight,
            l2Regularizer,
            sumAllWeights,
            allDocCount
        );
        falseAvrg = CalcAverage(
            falseStats.SumWeightedDelta,
            falseStats.SumWeight,
            l2Regularizer,
            sumAllWeights,
            allDocCount
        );
    } else {
        trueAvrg = CalcAverage(
            trueStats.SumDelta,
            trueStats.Count,
            l2Regularizer,
            sumAllWeights,
            allDocCount
        );
        falseAvrg = CalcAverage(
            falseStats.SumDelta,
            falseStats.Count,
            l2Regularizer,
            sumAllWeights,
            allDocCount
        );
    }
    (*scoreBin).DP += CountDp(trueAvrg, trueStats) + CountDp(falseAvrg, falseStats);
    (*scoreBin).D2 += CountD2(trueAvrg, trueStats) + CountD2(falseAvrg, falseStats);
}



/* This function calculates resulting sums for each split given statistics that are calculated for each bucket
 * of the histogram.
 */
template <typename TIsPlainMode>
inline static void UpdateScoreBins(
    const TBucketStats* stats,
    int leafCount,
    const TStatsIndexer& indexer,
    const TSplitEnsembleSpec& splitEnsembleSpec,
    float l2Regularizer,
    TIsPlainMode isPlainMode,
    double sumAllWeights,
    int allDocCount,
    TVector<TScoreBin>* scoreBins
) {
    auto updateScoreBinClosure = [=] (
        const TBucketStats& trueStats,
        const TBucketStats& falseStats,
        TScoreBin* scoreBin) {

        UpdateScoreBin(
            isPlainMode,
            l2Regularizer,
            sumAllWeights,
            allDocCount,
            trueStats,
            falseStats,
            scoreBin);
    };

    for (int leaf = 0; leaf < leafCount; ++leaf) {
        if (splitEnsembleSpec.IsBinarySplitsPack) {
            int binaryFeaturesCount = (int)GetValueBitCount(indexer.BucketCount - 1);
            for (int binFeatureIdx = 0; binFeatureIdx < binaryFeaturesCount; ++binFeatureIdx) {
                TBucketStats trueStats{0, 0, 0, 0};
                TBucketStats falseStats{0, 0, 0, 0};

                for (int bucketIdx = 0; bucketIdx < indexer.BucketCount; ++bucketIdx) {
                    auto& dstStats = ((bucketIdx >> binFeatureIdx) & 1) ? trueStats : falseStats;
                    dstStats.Add(stats[indexer.GetIndex(leaf, bucketIdx)]);
                }

                updateScoreBinClosure(trueStats, falseStats, &((*scoreBins)[binFeatureIdx]));
            }
        } else {
            auto splitType = splitEnsembleSpec.SplitType;

            TBucketStats allStats{0, 0, 0, 0};

            for (int bucketIdx = 0; bucketIdx < indexer.BucketCount; ++bucketIdx) {
                const TBucketStats& leafStats = stats[indexer.GetIndex(leaf, bucketIdx)];
                allStats.Add(leafStats);
            }

            TBucketStats trueStats{0, 0, 0, 0};
            TBucketStats falseStats{0, 0, 0, 0};
            if (splitType == ESplitType::OnlineCtr || splitType == ESplitType::FloatFeature) {
                trueStats = allStats;
                for (int splitIdx = 0; splitIdx < indexer.BucketCount - 1; ++splitIdx) {
                    falseStats.Add(stats[indexer.GetIndex(leaf, splitIdx)]);
                    trueStats.Remove(stats[indexer.GetIndex(leaf, splitIdx)]);

                    updateScoreBinClosure(trueStats, falseStats, &((*scoreBins)[splitIdx]));
                }
            } else {
                Y_ASSERT(splitType == ESplitType::OneHotFeature);
                falseStats = allStats;
                for (int bucketIdx = 0; bucketIdx < indexer.BucketCount; ++bucketIdx) {
                    if (bucketIdx > 0) {
                        falseStats.Add(stats[indexer.GetIndex(leaf, bucketIdx - 1)]);
                    }
                    falseStats.Remove(stats[indexer.GetIndex(leaf, bucketIdx)]);

                    updateScoreBinClosure(
                        /*trueStats*/ stats[indexer.GetIndex(leaf, bucketIdx)],
                        falseStats,
                        &((*scoreBins)[bucketIdx]));
                }
            }
        }
    }
}


static inline int CalcScoreBinCount(
    const TSplitEnsembleSpec& splitEnsembleSpec,
    int bucketCount
) {
    if (splitEnsembleSpec.IsBinarySplitsPack) {
        return GetValueBitCount(bucketCount - 1);
    }
    if (splitEnsembleSpec.SplitType == ESplitType::OneHotFeature) {
        return bucketCount;
    }
    // FloatFeature or Ctr
    return bucketCount - 1;
}


static void CalculateNonPairwiseScore(
    const TCalcScoreFold& fold,
    const TFold& initialFold,
    const TSplitEnsembleSpec& splitEnsembleSpec,
    bool isPlainMode,
    const int leafCount,
    const float l2Regularizer,
    const TStatsIndexer& indexer,
    const TBucketStats* splitStats,
    int splitStatsCount,
    TVector<TScoreBin>* scoreBins
) {
    const int approxDimension = fold.GetApproxDimension();

    scoreBins->assign(CalcScoreBinCount(splitEnsembleSpec, indexer.BucketCount), TScoreBin());

    for (int bodyTailIdx = 0; bodyTailIdx < fold.GetBodyTailCount(); ++bodyTailIdx) {
        double sumAllWeights = initialFold.BodyTailArr[bodyTailIdx].BodySumWeight;
        int docCount = initialFold.BodyTailArr[bodyTailIdx].BodyFinish;
        for (int dim = 0; dim < approxDimension; ++dim) {
            const TBucketStats* stats = splitStats
                + (bodyTailIdx * approxDimension + dim) * splitStatsCount;
            if (isPlainMode) {
                UpdateScoreBins(
                    stats,
                    leafCount,
                    indexer,
                    splitEnsembleSpec,
                    l2Regularizer,
                    /*isPlainMode=*/std::true_type(),
                    sumAllWeights,
                    docCount,
                    scoreBins
                );
            } else {
                UpdateScoreBins(
                    stats,
                    leafCount,
                    indexer,
                    splitEnsembleSpec,
                    l2Regularizer,
                    /*isPlainMode=*/std::false_type(),
                    sumAllWeights,
                    docCount,
                    scoreBins
                );
            }
        }
    }
}


void CalcStatsAndScores(
    const TQuantizedForCPUObjectsDataProvider& objectsDataProvider,
    const std::tuple<const TOnlineCTRHash&, const TOnlineCTRHash&>& allCtrs,
    const TCalcScoreFold& fold,
    const TCalcScoreFold& prevLevelData,
    const TFold* initialFold,
    const TFlatPairsInfo& pairs,
    const NCatboostOptions::TCatBoostOptions& fitParams,
    const TSplitEnsemble& splitEnsemble,
    int depth,
    bool useTreeLevelCaching,
    NPar::TLocalExecutor* localExecutor,
    TBucketStatsCache* statsFromPrevTree,
    TStats3D* stats3d,
    TPairwiseStats* pairwiseStats,
    TVector<TScoreBin>* scoreBins
) {
    CB_ENSURE(stats3d || pairwiseStats || scoreBins, "stats3d, pairwiseStats, and scoreBins are empty - nothing to calculate");
    CB_ENSURE(!scoreBins || initialFold, "initialFold must be non-nullptr for scoreBins calculation");

    const bool isPairwiseScoring = IsPairwiseScoring(fitParams.LossFunctionDescription->GetLossFunction());

    const int bucketCount = GetBucketCount(
        splitEnsemble,
        *objectsDataProvider.GetQuantizedFeaturesInfo(),
        objectsDataProvider.GetPackedBinaryFeaturesSize()
    );
    const TStatsIndexer indexer(bucketCount);
    const int fullIndexBitCount = depth + GetValueBitCount(bucketCount - 1);
    const bool isPlainMode = IsPlainMode(fitParams.BoostingOptions->BoostingType);

    const float l2Regularizer = static_cast<float>(fitParams.ObliviousTreeOptions->L2Reg);

    decltype(auto) selectCalcStatsImpl = [&] (
        auto isCaching,
        const TCalcScoreFold& fold,
        int splitStatsCount,
        auto* stats
    ) {
        if (fullIndexBitCount <= 8) {
            CalcStatsImpl<ui8>(
                fold,
                objectsDataProvider,
                pairs,
                allCtrs,
                splitEnsemble,
                indexer,
                isCaching,
                isPlainMode,
                depth,
                splitStatsCount,
                localExecutor,
                stats
            );
        } else if (fullIndexBitCount <= 16) {
            CalcStatsImpl<ui16>(
                fold,
                objectsDataProvider,
                pairs,
                allCtrs,
                splitEnsemble,
                indexer,
                isCaching,
                isPlainMode,
                depth,
                splitStatsCount,
                localExecutor,
                stats
            );
        } else if (fullIndexBitCount <= 32) {
            CalcStatsImpl<ui32>(
                fold,
                objectsDataProvider,
                pairs,
                allCtrs,
                splitEnsemble,
                indexer,
                isCaching,
                isPlainMode,
                depth,
                splitStatsCount,
                localExecutor,
                stats
            );
        }
    };

    // Pairwise scoring doesn't use statistics from previous tree level
    if (isPairwiseScoring) {
        CB_ENSURE(!stats3d, "Pairwise scoring is incompatible with stats3d calculation");

        TPairwiseStats localPairwiseStats;
        if (pairwiseStats == nullptr) {
            pairwiseStats = &localPairwiseStats;
        }
        selectCalcStatsImpl(/*isCaching*/ std::false_type(), fold, /*splitStatsCount*/0, pairwiseStats);

        if (scoreBins) {
            const float pairwiseBucketWeightPriorReg =
                static_cast<float>(fitParams.ObliviousTreeOptions->PairwiseNonDiagReg);
            CalculatePairwiseScore(
                *pairwiseStats,
                bucketCount,
                l2Regularizer,
                pairwiseBucketWeightPriorReg,
                scoreBins
            );
        }
    } else {
        CB_ENSURE(!pairwiseStats, "Per-object scoring is incompatible with pairwiseStats calculation");
        TBucketStatsRefOptionalHolder extOrInSplitStats;
        int splitStatsCount = 0;

        const auto& treeOptions = fitParams.ObliviousTreeOptions.Get();

        if (!useTreeLevelCaching) {
            splitStatsCount = indexer.CalcSize(depth);
            const int statsCount =
                fold.GetBodyTailCount() * fold.GetApproxDimension() * splitStatsCount;

            if (stats3d != nullptr) {
                stats3d->Stats.yresize(statsCount);
                stats3d->BucketCount = bucketCount;
                stats3d->MaxLeafCount = 1U << depth;
                stats3d->SplitEnsembleSpec = TSplitEnsembleSpec(splitEnsemble);

                extOrInSplitStats = TBucketStatsRefOptionalHolder(stats3d->Stats);
            }
            selectCalcStatsImpl(
                /*isCaching*/ std::false_type(),
                fold,
                splitStatsCount,
                &extOrInSplitStats
            );
        } else {
            splitStatsCount = indexer.CalcSize(treeOptions.MaxDepth);
            bool areStatsDirty;
            TVector<TBucketStats, TPoolAllocator>& splitStatsFromCache =
                statsFromPrevTree->GetStats(splitEnsemble, splitStatsCount, &areStatsDirty); // thread-safe access
            extOrInSplitStats = TBucketStatsRefOptionalHolder(splitStatsFromCache);
            if (depth == 0 || areStatsDirty) {
                selectCalcStatsImpl(
                    /*isCaching*/ std::false_type(),
                    fold,
                    splitStatsCount,
                    &extOrInSplitStats
                );
            } else {
                selectCalcStatsImpl(
                    /*isCaching*/ std::true_type(),
                    prevLevelData,
                    splitStatsCount,
                    &extOrInSplitStats
                );
            }
            if (stats3d) {
                TBucketStatsCache::GetStatsInUse(fold.GetBodyTailCount() * fold.GetApproxDimension(),
                    splitStatsCount,
                    indexer.CalcSize(depth),
                    splitStatsFromCache
                ).swap(stats3d->Stats);
                stats3d->BucketCount = bucketCount;
                stats3d->MaxLeafCount = 1U << depth;
                stats3d->SplitEnsembleSpec = TSplitEnsembleSpec(splitEnsemble);
            }
        }
        if (scoreBins) {
            const int leafCount = 1 << depth;
            CalculateNonPairwiseScore(
                fold,
                *initialFold,
                TSplitEnsembleSpec(splitEnsemble),
                isPlainMode,
                leafCount,
                l2Regularizer,
                indexer,
                extOrInSplitStats.GetData().Data(),
                splitStatsCount,
                scoreBins
            );
        }
    }
}

#if defined(SCORE_BIN_TLS)
const TVector<TScoreBin>&
#else
TVector<TScoreBin>
#endif
GetScoreBins(
    const TStats3D& stats3d,
    int depth,
    double sumAllWeights,
    int allDocCount,
    const NCatboostOptions::TCatBoostOptions& fitParams
) {
    const TVector<TBucketStats>& bucketStats = stats3d.Stats;
    const int splitStatsCount = stats3d.BucketCount * stats3d.MaxLeafCount;
    const int bucketCount = stats3d.BucketCount;
    const float l2Regularizer = static_cast<float>(fitParams.ObliviousTreeOptions->L2Reg);
    const int leafCount = 1 << depth;
    const TStatsIndexer indexer(bucketCount);
#if defined(SCORE_BIN_TLS)
    Y_STATIC_THREAD(TVector<TScoreBin>) scoreBinLocal; // TVector is non-POD
    TVector<TScoreBin>& scoreBin = TlsRef(scoreBinLocal);
    scoreBin.resize(bucketCount);
#else
    TVector<TScoreBin> scoreBin(bucketCount);
#endif
    for (int statsIdx = 0; statsIdx * splitStatsCount < bucketStats.ysize(); ++statsIdx) {
        const TBucketStats* stats = GetDataPtr(bucketStats) + statsIdx * splitStatsCount;
        UpdateScoreBins(
            stats,
            leafCount,
            indexer,
            stats3d.SplitEnsembleSpec,
            l2Regularizer,
            /*isPlainMode=*/std::true_type(),
            sumAllWeights,
            allDocCount,
            &scoreBin
        );
    }
    return scoreBin;
}
