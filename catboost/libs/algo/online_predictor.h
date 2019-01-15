#pragma once

#include "hessian.h"

#include <catboost/libs/options/enums.h>

#include <library/binsaver/bin_saver.h>
#include <library/containers/2d_array/2d_array.h>

#include <util/generic/vector.h>
#include <util/system/yassert.h>


struct TSum {
    double SumDer = 0.0;
    double SumDer2 = 0.0;
    double SumWeights = 0.0;

    explicit TSum(int approxDimension = 1, EHessianType hessianType = EHessianType::Symmetric) {
        Y_ASSERT(approxDimension == 1);
        Y_ASSERT(hessianType == EHessianType::Symmetric);
    }

    bool operator==(const TSum& other) const {
        return SumDer == other.SumDer &&
               SumWeights == other.SumWeights &&
               SumDer2 == other.SumDer2;
    }

    inline void SetZeroDers() {
        SumDer = 0.0;
        SumDer2 = 0.0;
    }

    inline void AddDerWeight(double delta, double weight, int gradientIteration) {
        SumDer += delta;
        if (gradientIteration == 0) {
            SumWeights += weight;
        }
    }

    inline void AddDerDer2(double delta, double der2) {
        SumDer += delta;
        SumDer2 += der2;
    }
    SAVELOAD(SumDer, SumDer2, SumWeights);
};

struct TSumMulti {
    TVector<double> SumDer; // [approxIdx]
    THessianInfo SumDer2; // [approxIdx1][approxIdx2]
    double SumWeights = 0.0;

    TSumMulti() = default;

    explicit TSumMulti(int approxDimension, EHessianType hessianType)
    : SumDer(approxDimension)
    , SumDer2(approxDimension, hessianType)
    {}

    bool operator==(const TSumMulti& other) const {
        return SumDer == other.SumDer &&
               SumWeights == other.SumWeights &&
               SumDer2 == other.SumDer2;
    }

    inline void SetZeroDers() {
        Fill(SumDer.begin(), SumDer.end(),  0.0);
        Fill(SumDer2.Data.begin(), SumDer2.Data.end(), 0.0);
    }

    void AddDerWeight(const TVector<double>& delta, double weight, int gradientIteration) {
        Y_ASSERT(delta.ysize() == SumDer.ysize());
        for (int dim = 0; dim < SumDer.ysize(); ++dim) {
            SumDer[dim] += delta[dim];
        }
        if (gradientIteration == 0) {
            SumWeights += weight;
        }
    }

    void AddDerDer2(const TVector<double>& delta, const THessianInfo& der2) {
        Y_ASSERT(delta.ysize() == SumDer.ysize());
        for (int dim = 0; dim < SumDer.ysize(); ++dim) {
            SumDer[dim] += delta[dim];
        }
        SumDer2.AddDer2(der2);
    }
    SAVELOAD(SumDer, SumDer2, SumWeights);
};

inline double CalcAverage(double sumDelta,
                          double count,
                          float l2Regularizer,
                          double sumAllWeights,
                          int allDocCount) {
    double inv = count > 0 ? 1. / (count + l2Regularizer * (sumAllWeights / allDocCount)) : 0;
    return sumDelta * inv;
}

inline double CalcModelGradient(const TSum& ss,
                                float l2Regularizer,
                                double sumAllWeights,
                                int allDocCount) {
    return CalcAverage(ss.SumDer,
                       ss.SumWeights,
                       l2Regularizer,
                       sumAllWeights,
                       allDocCount);
}

inline void CalcModelGradientMulti(const TSumMulti& ss,
                                   float l2Regularizer,
                                   double sumAllWeights,
                                   int allDocCount,
                                   TVector<double>* res) {
    const int approxDimension = ss.SumDer.ysize();
    res->resize(approxDimension);
    for (int dim = 0; dim < approxDimension; ++dim) {
        (*res)[dim] = CalcAverage(ss.SumDer[dim],
                                  ss.SumWeights,
                                  l2Regularizer,
                                  sumAllWeights,
                                  allDocCount);
    }
}

inline double CalcModelNewtonBody(double sumDer,
                                  double sumDer2,
                                  float l2Regularizer,
                                  double sumAllWeights,
                                  int allDocCount) {
    return sumDer / (-sumDer2 + l2Regularizer * (sumAllWeights / allDocCount));
}

inline double CalcModelNewton(const TSum& ss,
                              float l2Regularizer,
                              double sumAllWeights,
                              int allDocCount) {
    return CalcModelNewtonBody(ss.SumDer,
                               ss.SumDer2,
                               l2Regularizer,
                               sumAllWeights,
                               allDocCount);
}

void CalcModelNewtonMulti(const TSumMulti& ss,
                          float l2Regularizer,
                          double sumAllWeights,
                          int allDocCount,
                          TVector<double>* res);
