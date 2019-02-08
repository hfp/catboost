#pragma once

#include "exception.h"

#include <catboost/libs/index_range/index_range.h>

#include <library/threading/local_executor/local_executor.h>

#include <util/generic/vector.h>
#include <util/generic/utility.h>
#include <util/generic/ymath.h>

#if !defined(MAP_MERGE_FASTREDUCE)
# define MAP_MERGE_FASTREDUCE
#endif

namespace NCB {

    /**
     * Processes data in parallel by blocks then merges results
     *  if there is only one block then there's no merge
     *
     * indexRange determine source data ranges for mapFunc input
     *   (mapFunc knows itself how source data is indexed)
     * mapFunc(blockIndexRange, blockOutput) processes data in range
     *   blockIndexRange and saves data to blockOutput
     *   should be able to handle empty index range (return valid blockOutput in this case)
     * mergeFunc(dst, addVector) adds addVector data to dst
     */
    template <class TOutput, class TMapFunc, class TMergeFunc>
    void MapMerge(
        NPar::TLocalExecutor* localExecutor,
        const IIndexRangesGenerator<int>& indexRangesGenerator,
        TMapFunc&& mapFunc, // void(NCB::TIndexRange, TOutput*)
        TMergeFunc&& mergeFunc, // void(TOutput*, TVector<TOutput>&&)
        TOutput* output
    ) {
        int blockCount = indexRangesGenerator.RangesCount();

        if (blockCount == 0) {
            mapFunc(NCB::TIndexRange<int>(0), output);
        } else if (blockCount == 1) {
            mapFunc(indexRangesGenerator.GetRange(0), output);
        } else {
#if defined(MAP_MERGE_FASTREDUCE)
            static TVector<TOutput> mapOutputs; // w/o first, first is reused from 'output' param
            mapOutputs.yresize(blockCount - 1);
#else
            TVector<TOutput> mapOutputs(blockCount - 1); // w/o first, first is reused from 'output' param
#endif
            localExecutor->ExecRange(
                [&](int blockId) {
                    mapFunc(
                        indexRangesGenerator.GetRange(blockId),
                        (blockId == 0) ? output : &(mapOutputs[blockId - 1])
                    );
                },
                0,
                blockCount,
                NPar::TLocalExecutor::WAIT_COMPLETE
            );

            mergeFunc(output, mapOutputs);
        }
    }

}
