LIBRARY()



SRCS(
    apply.cpp
    approx_calcer.cpp
    approx_calcer_helpers.cpp
    approx_calcer_multi.cpp
    approx_calcer_querywise.cpp
    approx_updater_helpers.cpp
    bin_tracker.cpp
    calc_score_cache.cpp
    ctr_helper.cpp
    custom_objective_descriptor.cpp
    ders_holder.cpp
    error_functions.cpp
    features_data_helpers.cpp
    fold.cpp
    full_model_saver.cpp
    greedy_tensor_search.cpp
    helpers.cpp
    hessian.cpp
    index_calcer.cpp
    index_hash_calcer.cpp
    learn_context.cpp
    mvs.cpp
    online_ctr.cpp
    online_predictor.cpp
    plot.cpp
    projection.cpp
    rand_score.cpp
    score_bin.cpp
    score_calcer.cpp
    split.cpp
    target_classifier.cpp
    tensor_search_helpers.cpp
    train.cpp
    tree_print.cpp
    yetirank_helpers.cpp
    pairwise_leaves_calculation.cpp
    pairwise_scoring.cpp
    roc_curve.cpp
)

PEERDIR(
    catboost/libs/cat_feature
    catboost/libs/data_new
    catboost/libs/data_types
    catboost/libs/eval_result
    catboost/libs/helpers
    catboost/libs/index_range
    catboost/libs/labels
    catboost/libs/lapack
    catboost/libs/loggers
    catboost/libs/logging
    catboost/libs/metrics
    catboost/libs/model
    catboost/libs/target
    catboost/libs/options
    catboost/libs/overfitting_detector
    library/binsaver
    library/containers/2d_array
    library/containers/dense_hash
    library/containers/stack_vector
    library/digest/crc32c
    library/digest/md5
    library/dot_product
    library/fast_exp
    library/fast_log
    library/grid_creator
    library/json
    library/malloc/api
    library/object_factory
    library/par
    library/svnversion
    library/threading/local_executor
)

IF (TBB)
    PEERDIR(contrib/libs/tbb)
ELSE()
    ADDINCL(GLOBAL contrib/libs)
ENDIF()

END()
