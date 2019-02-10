#pragma once

#include <library/binsaver/bin_saver.h>

#include <util/generic/vector_ops.h>
#include <util/system/yassert.h>

// TODO(annaveronika): each metric should implement CreateMetricHolder(), CombineMetricHolders()
struct TMetricHolder: public NVectorOps::TVectorOps<double, TMetricHolder> {
    static constexpr int StatsCapacity = 4;
    value_type Stats[StatsCapacity];
    int StatsCount;

    explicit TMetricHolder(int statsCount = 0) : StatsCount(statsCount) {
        Y_VERIFY(StatsCount <= StatsCapacity);
    }

    void Add(const TMetricHolder& other) {
        Y_VERIFY(Stats.empty() || other.Stats.empty() || Stats.size() == other.Stats.size());
        Y_VERIFY(StatsCount <= StatsCapacity && other.StatsCount <= other.StatsCapacity);
        if (!other.Stats.empty()) {
            if (Stats.empty()) {
                for (int i = 0; i < other.StatsCount; ++i) {
                    Stats[i] = other.Stats[i];
                }
                StatsCount = other.StatsCount
            }
            else {
                for (int i = 0; i < StatsCount; ++i) {
                    Stats[i] += other.Stats[i];
                }              
            }
        }
    }
    SAVELOAD(Stats);
};

