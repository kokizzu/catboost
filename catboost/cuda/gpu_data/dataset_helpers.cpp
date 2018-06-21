#include "dataset_helpers.h"

THolder<NCatboostCuda::TCtrTargets<NCudaLib::TMirrorMapping>> NCatboostCuda::BuildCtrTarget(const NCatboostCuda::TBinarizedFeaturesManager& featuresManager,
                                                                                            const NCatboostCuda::TDataProvider& dataProvider,
                                                                                            const NCatboostCuda::TDataProvider* test)  {
    TVector<float> joinedTarget = Join(dataProvider.GetTargets(),
                                       test ? &test->GetTargets() : nullptr);

    THolder<TCtrTargets<NCudaLib::TMirrorMapping>> ctrsTargetPtr;
    ctrsTargetPtr = new TCtrTargets<NCudaLib::TMirrorMapping>;
    auto& ctrsTarget = *ctrsTargetPtr;
    ctrsTarget.BinarizedTarget = BuildBinarizedTarget(featuresManager,
                                                      joinedTarget);

    ctrsTarget.WeightedTarget.Reset(NCudaLib::TMirrorMapping(joinedTarget.size()));
    ctrsTarget.Weights.Reset(NCudaLib::TMirrorMapping(joinedTarget.size()));

    ctrsTarget.LearnSlice = TSlice(0, dataProvider.GetSampleCount());
    ctrsTarget.TestSlice = TSlice(dataProvider.GetSampleCount(), joinedTarget.size());

    TVector<float> ctrWeights;
    ctrWeights.resize(joinedTarget.size(), 1.0f);

    TVector<float> ctrWeightedTargets(joinedTarget.begin(), joinedTarget.end());

    double totalWeight = 0;
    for (ui32 i = (ui32)ctrsTarget.LearnSlice.Right; i < ctrWeights.size(); ++i) {
        ctrWeights[i] = 0;
    }

    for (ui32 i = 0; i < ctrWeightedTargets.size(); ++i) {
        ctrWeightedTargets[i] *= ctrWeights[i];
        totalWeight += ctrWeights[i];
    }

    ctrsTarget.TotalWeight = (float)totalWeight;
    ctrsTarget.WeightedTarget.Write(ctrWeightedTargets);
    ctrsTarget.Weights.Write(ctrWeights);

    CB_ENSURE(ctrsTarget.IsTrivialWeights());
    return ctrsTargetPtr;
}

TVector<ui32> NCatboostCuda::GetLearnFeatureIds(NCatboostCuda::TBinarizedFeaturesManager& featuresManager) {
    TSet<ui32> featureIdsSet;
    auto ctrTypes = featuresManager.GetKnownSimpleCtrTypes();

    for (auto floatFeature : featuresManager.GetFloatFeatureIds()) {
        if (featuresManager.GetBinCount(floatFeature) > 1) {
            featureIdsSet.insert(floatFeature);
        }
    }
    for (auto catFeature : featuresManager.GetCatFeatureIds()) {
        if (featuresManager.UseForOneHotEncoding(catFeature)) {
            if (featuresManager.GetBinCount(catFeature) > 1) {
                featureIdsSet.insert(catFeature);
            }
        }

        if (featuresManager.UseForCtr(catFeature)) {
            for (auto& ctr : ctrTypes) {
                const auto simpleCtrsForType = featuresManager.CreateSimpleCtrsForType(catFeature,
                                                                                       ctr);
                for (auto ctrFeatureId : simpleCtrsForType) {
                    featureIdsSet.insert(ctrFeatureId);
                }
            }
        }
    }
    TSet<ui32> combinationCtrIds;

    for (auto& ctr : ctrTypes) {
        auto combinationCtrs = featuresManager.CreateCombinationCtrForType(ctr);
        for (auto ctrFeatureId : combinationCtrs) {
            TFeatureTensor tensor = featuresManager.GetCtr(ctrFeatureId).FeatureTensor;
            bool hasUnknownFeatures = false;
            CB_ENSURE(tensor.GetSplits().size() == 0);

            for (auto featureId : tensor.GetCatFeatures()) {
                if (!featureIdsSet.has(featureId)) {
                    hasUnknownFeatures = true;
                    break;
                }
            }
            for (auto binarySplit : tensor.GetSplits()) {
                if (!featureIdsSet.has(binarySplit.FeatureId)) {
                    hasUnknownFeatures = true;
                    break;
                }
            }
            if (!hasUnknownFeatures) {
                combinationCtrIds.insert(ctrFeatureId);
            }
        }
    }
    featureIdsSet.insert(combinationCtrIds.begin(), combinationCtrIds.end());
    return TVector<ui32>(featureIdsSet.begin(), featureIdsSet.end());
}

namespace NCatboostCuda {
     template class TFloatAndOneHotFeaturesWriter<TFeatureParallelLayout>;
     template class TFloatAndOneHotFeaturesWriter<TDocParallelLayout>;

     template class TCtrsWriter<TFeatureParallelLayout>;
     template class TCtrsWriter<TDocParallelLayout>;
}
