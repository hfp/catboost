#pragma once

#include <util/generic/bitops.h>

#include <cmath>


namespace NCB {
    inline ui32 IntLog2(ui32 value) {
        const ui32 v0 = MostSignificantBit(value - 1), v1 = MostSignificantBit(value);
        const ui32 result = (1 < value ? (v0 != v1 ? v1 : (v1 + 1)) : 0);
        Y_ASSERT(result == (ui32)ceil(log2(value)));
        return result;
    }

    // nan == nan is true here in contrast with the standard comparison operator
    template <class T>
    inline bool EqualWithNans(T lhs, T rhs) {
        if (std::isnan(lhs)) {
            return std::isnan(rhs);
        }
        return lhs == rhs;
    }
}

