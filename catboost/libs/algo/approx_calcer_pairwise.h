#pragma once

#include "approx_calcer_helpers.h"
#include "approx_util.h"

template <typename TError>
void CalcShiftedApproxDersPairs(const TVector<double>& approx,
                                const TVector<double>& approxDelta,
                                const TVector<TVector<TCompetitor>>& competitors,
                                const TError& error,
                                int sampleStart,
                                int sampleFinish,
                                TVector<TDer1Der2>* scratchDers) {
    TVector<double> fullApproxes(approx);
    if (!approxDelta.empty()) {
        for (int docId = 0; docId < sampleFinish; ++docId) {
            fullApproxes[docId] = UpdateApprox<TError::StoreExpApprox>(approx[docId], approxDelta[docId]);
        }
    }

    const int dersSize = sampleFinish - sampleStart;
    TVector<double> ders(dersSize);
    error.CalcDersPairs(fullApproxes, competitors, sampleStart, dersSize, &ders);
    for (int docId = 0; docId < dersSize; ++docId) {
        (*scratchDers)[docId].Der1 = ders[docId];
    }
}

template <typename TError>
void CalcApproxDersRangePairs(const TVector<TIndexType>& indices,
                              const TVector<double>& approx,
                              const TVector<double>& approxDelta,
                              const TVector<TVector<TCompetitor>>& competitors,
                              const TError& error,
                              int sampleCount,
                              int sampleTotal,
                              int iteration,
                              TVector<TSum>* buckets,
                              TVector<TDer1Der2>* scratchDers) {
    const int leafCount = buckets->ysize();

    TVector<double> fullApproxes(approx);
    if (!approxDelta.empty()) {
        for (int docId = 0; docId < sampleTotal; ++docId) {
            fullApproxes[docId] = UpdateApprox<TError::StoreExpApprox>(approx[docId], approxDelta[docId]);
        }
    }

    TVector<double> ders(sampleCount);
    error.CalcDersPairs(fullApproxes, competitors, 0, sampleCount, &ders);
    for (int docId = 0; docId < sampleCount; ++docId) {
        (*scratchDers)[docId].Der1 = ders[docId];
    }

    TVector<TDer1Der2> bucketDers(leafCount, TDer1Der2{/*Der1*/0.0, /*Der2*/0.0 });
    TVector<double> bucketWeights(leafCount, 0);
    for (int docId = 0; docId < sampleCount; ++docId) {
        TDer1Der2& currentDers = bucketDers[indices[docId]];
        currentDers.Der1 += ders[docId];
        bucketWeights[indices[docId]] += 1;
    }

    for (int leafId = 0; leafId < leafCount; ++leafId) {
        if (bucketWeights[leafId] > FLT_EPSILON) {
            UpdateBucket<ELeavesEstimation::Gradient>(bucketDers[leafId], bucketWeights[leafId], iteration, &(*buckets)[leafId]);
        }
    }
}
