#pragma once

#include <library/binsaver/bin_saver.h>
#include <util/system/yassert.h>

#if !defined(METRIC_HOLDER_STATIC)
# include <util/generic/vector_ops.h>
# define METRIC_HOLDER_STATIC
#else
# include <util/generic/vector.h>
#endif

// TODO(annaveronika): each metric should implement CreateMetricHolder(), CombineMetricHolders()
struct TMetricHolder {
#if !defined(METRIC_HOLDER_STATIC)
    explicit TMetricHolder(int statsCount = 0) : Stats(statsCount) {}
    TVector<double> Stats;
#else
    TVectorOps<double, double[4]> Stats;
    explicit TMetricHolder(int statsCount = 0) noexcept {
        Stats.Resize(statsCount);
        for (int i = 0; i < Stats.ycapacity(); ++i) {
            Stats[i] = 0;
        }
    }
#endif
    void Add(const TMetricHolder& other) {
        Y_VERIFY(Stats.empty() || other.Stats.empty() || Stats.size() == other.Stats.size());
        if (!other.Stats.empty()) {
            if (Stats.Empty()) {
                Stats.Resize(other.Stats.Size());
#if defined(METRIC_HOLDER_STATIC)
                for (int i = 0; i < other.Stats.ycapacity(); ++i) {
#else
                for (int i = 0; i < other.Stats.ysize(); ++i) {
#endif
                    Stats[i] = other.Stats[i];
                }
            }
            else {
#if defined(METRIC_HOLDER_STATIC)
                for (int i = 0; i < other.Stats.ycapacity(); ++i) {
#else
                for (int i = 0; i < other.Stats.ysize(); ++i) {
#endif
                    Stats[i] += other.Stats[i];
                }
            }
        }
    }
    SAVELOAD(Stats);
};

