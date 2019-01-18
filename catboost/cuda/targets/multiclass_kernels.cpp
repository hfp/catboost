#include "multiclass_kernels.h"

using namespace NKernelHost;

namespace NCudaLib {
    REGISTER_KERNEL(0xA11BB00, TMultiLogitValueAndDerKernel);
    REGISTER_KERNEL(0xA11BB01, TMultiLogitSecondDerKernel);

    REGISTER_KERNEL(0xA11BB02, TMultiClassOneVsAllValueAndDerKernel);
    REGISTER_KERNEL(0xA11BB03, TMultiClassOneVsAllSecondDerKernel);
    REGISTER_KERNEL(0xA11BB04, TBuildConfusionMatrixKernel);
}
