#pragma once

#include <util/generic/vector.h>
#include <util/generic/xrange.h>
#include <util/generic/ymath.h>
#include <util/system/yassert.h>
#if defined(__TBB)
# include <tbb/tbb.h>
#endif

// TODO(akhropov): move back to libs/helpers when circular dependencies with libs/data_types are resolved

namespace NCB {

    // represents index range to process: [Begin, End)
    template <class TSize>
    struct TIndexRange {
        TSize Begin = 0;
        TSize End = 0;

    public:
        // for BinSaver
        TIndexRange() = default;

        explicit TIndexRange(TSize end)
            : TIndexRange(TSize(0), end)
        {}

        TIndexRange(TSize begin, TSize end)
            : Begin(begin)
            , End(end)
        {
            Y_ASSERT(End >= Begin);
        }

        bool Empty() const {
            return Begin == End;
        }

        TSize GetSize() const {
            Y_ASSERT(End >= Begin);
            return End - Begin;
        }

        bool operator==(const TIndexRange& rhs) const {
            return (Begin == rhs.Begin) && (End == rhs.End);
        }

        // support for range-based for loop
        constexpr auto Iter() const {
            return xrange(Begin, End);
        }
#if defined(__TBB)
        constexpr auto IterParallel() const {
            return tbb::blocked_range<TSize>(Begin, End);
        }
#endif
    };

    template <class TSize>
    struct IIndexRangesGenerator {
        virtual ~IIndexRangesGenerator() = default;

        virtual TSize RangesCount() const = 0;

        virtual NCB::TIndexRange<TSize> GetRange(TSize idx) const = 0;
    };

    template <class TSize>
    class TSimpleIndexRangesGenerator : public IIndexRangesGenerator<TSize> {
    public:
        TSimpleIndexRangesGenerator(NCB::TIndexRange<TSize> fullRange, TSize blockSize)
            : FullRange(fullRange)
            , BlockSize(blockSize)
        {}

        TSize RangesCount() const override {
            return CeilDiv(FullRange.GetSize(), BlockSize);
        }

        NCB::TIndexRange<TSize> GetRange(TSize idx) const override {
            Y_ASSERT(idx < RangesCount());
            TSize blockBeginIdx = FullRange.Begin + idx*BlockSize;
            TSize blockEndIdx = Min(blockBeginIdx + BlockSize, FullRange.End);
            return NCB::TIndexRange<TSize>(blockBeginIdx, blockEndIdx);
        }

    private:
        NCB::TIndexRange<TSize> FullRange;
        TSize BlockSize;
    };

    template <class TSize>
    class TSavedIndexRanges : public NCB::IIndexRangesGenerator<TSize> {
    public:
        explicit TSavedIndexRanges(TVector<NCB::TIndexRange<TSize>>&& indexRanges)
            : IndexRanges(std::move(indexRanges))
        {}

        TSize RangesCount() const override {
            return (TSize)IndexRanges.size();
        }

        NCB::TIndexRange<TSize> GetRange(TSize idx) const override {
            return IndexRanges[idx];
        }

    private:
        TVector<NCB::TIndexRange<TSize>> IndexRanges;
    };

}
