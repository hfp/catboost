#pragma once

#include <library/binsaver/bin_saver.h>

#include <util/generic/vector_ops.h>
#include <util/system/yassert.h>

// TODO(annaveronika): each metric should implement CreateMetricHolder(), CombineMetricHolders()
struct TMetricHolder: public NVectorOps::TVectorOps<double, TMetricHolder> {
    TVectorOps<double, double[4]> Stats;

    explicit TMetricHolder(int statsCount = 0) noexcept {
        Stats.Resize(statsCount);
    }

    void Add(const TMetricHolder& other) {
        Y_VERIFY(Stats.Empty() || other.Stats.Empty() || Stats.Size() == other.Stats.Size());
        if (!other.Stats.Empty()) {
            if (Stats.Empty()) {
                Stats.Resize(other.Stats.Size());
                for (int i = 0; i < other.Stats.Size(); ++i) {
                    Stats[i] = other.Stats[i];
                }
            }
            else {
                for (int i = 0; i < Stats.Size(); ++i) {
                    Stats[i] += other.Stats[i];
                }
            }
        }
    }
    SAVELOAD(Stats);
};

