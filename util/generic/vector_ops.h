#pragma once

#include <util/system/yassert.h>
#include <util/generic/yexception.h>

namespace NVectorOps {
    template <class T, class TVec>
    class TVectorOpsBase {
        inline const TVec& Vec() const noexcept {
            return *static_cast<const TVec*>(this);
        }

    public:
        using TConstIterator = const T*;
        using TConstReference = const T&;
        using IVec = TVec;

        inline T* Data() const noexcept {
            return Vec().Data();
        }

        inline size_t Size() const noexcept {
            return Vec().Size();
        }

        Y_PURE_FUNCTION
        inline bool Empty() const noexcept {
            return !Size();
        }

        inline TConstIterator Begin() const noexcept {
            return Data();
        }

        inline TConstIterator End() const noexcept {
            return Data() + Size();
        }

        inline TConstReference Front() const noexcept {
            return (*this)[0];
        }

        inline TConstReference Back() const noexcept {
            Y_ASSERT(!Empty());

            return *(End() - 1);
        }

        inline TConstReference At(size_t n) const {
            if (n >= Size()) {
                ThrowRangeError("array ref range error");
            }

            return (*this)[n];
        }

        inline explicit operator bool() const noexcept {
            return !Empty();
        }

        inline T& operator[](size_t n) const noexcept {
            Y_ASSERT(n < Size());

            return *(Data() + n);
        }

        //compat, do not use
        using const_iterator = TConstIterator;
        using const_reference = TConstReference;
        using value_type = T;

        inline const_iterator begin() const noexcept {
            return Begin();
        }

        inline const_iterator end() const noexcept {
            return End();
        }

        inline size_t size() const noexcept {
            return Size();
        }

        Y_PURE_FUNCTION
        inline bool empty() const noexcept {
            return Empty();
        }

        inline const_reference front() const noexcept {
            return Front();
        }

        inline const_reference back() const noexcept {
            return Back();
        }

        inline const_reference at(size_t n) const {
            return At(n);
        }
    };

    template <class T, size_t N>
    class TVectorOpsBase<T, T[N]> {
        T buffer[N];
        size_t bufferSize;

    public:
        using TConstIterator = const T*;
        using TConstReference = const T*;
        using IVec = TVectorOpsBase;

        inline TVectorOpsBase() noexcept : bufferSize(N) {}

        inline T* Data() noexcept {
            return buffer;
        }

        inline const T* Data() const noexcept {
            return buffer;
        }

        inline constexpr static size_t Capacity() noexcept {
            return N;
        }

        inline size_t Size() const noexcept {
            return bufferSize;
        }

        inline void Resize(size_t size) noexcept {
            Y_ASSERT(size <= N);
            bufferSize = size;
        }

        Y_PURE_FUNCTION
        inline bool Empty() const noexcept {
            return 0 == bufferSize;
        }

        inline TConstIterator Begin() const noexcept {
            return Data();
        }

        inline TConstIterator End() const noexcept {
            return Data() + Size();
        }

        inline TConstReference Front() const noexcept {
            return (*this)[0];
        }

        inline TConstReference Back() const noexcept {
            Y_ASSERT(!Empty());
            return (*this)[bufferSize - 1];
        }

        inline TConstReference At(size_t n) const {
            if (n >= Size()) {
                ThrowRangeError("array ref range error");
            }
            return buffer[n];
        }

        inline explicit operator bool() const noexcept {
            return !Empty();
        }

        inline const T& operator[](size_t n) const noexcept {
            Y_ASSERT(n < Size());
            return buffer[n];
        }

        inline T& operator[](size_t n) noexcept {
            Y_ASSERT(n < Size());
            return buffer[n];
        }

        //compat, do not use
        using const_iterator = TConstIterator;
        using const_reference = TConstReference;
        using value_type = T;

        inline const_iterator begin() const noexcept {
            return Begin();
        }

        inline const_iterator end() const noexcept {
            return End();
        }

        inline constexpr static int ycapacity() noexcept {
            return static_cast<int>(Capacity());
        }

        inline int ysize() const noexcept {
            return static_cast<int>(Size());
        }

        inline size_t size() const noexcept {
            return Size();
        }

        inline void resize(size_t size) noexcept {
            this->Resize(size);
        }

        inline void yresize(int size) noexcept {
            Y_ASSERT(0 <= size);
            this->Resize(static_cast<size_t>(size));
        }

        Y_PURE_FUNCTION
        inline bool empty() const noexcept {
            return Empty();
        }

        inline const_reference front() const noexcept {
            return Front();
        }

        inline const_reference back() const noexcept {
            return Back();
        }

        inline const_reference at(size_t n) const {
            return At(n);
        }
    };

    template <class T, class TVec>
    class TVectorOps: public TVectorOpsBase<T, TVec> {
        using TBase = TVectorOpsBase<T, TVec>;
        using IVec = typename TBase::IVec;

        inline const IVec& Vec() const noexcept {
            return *static_cast<const IVec*>(this);
        }

    public:
        using TIterator = T*;
        using TReference = T&;

        using TBase::At;
        using TBase::Back;
        using TBase::Begin;
        using TBase::Data;
        using TBase::End;
        using TBase::Front;
        using TBase::operator[];

        inline T* Data() const noexcept {
            return Vec().Data();
        }

        inline TIterator Begin() noexcept {
            return this->Data();
        }

        inline TIterator End() noexcept {
            return this->Data() + this->Size();
        }

        inline TReference Front() noexcept {
            return (*this)[0];
        }

        inline TReference Back() noexcept {
            Y_ASSERT(!this->Empty());

            return *(this->End() - 1);
        }

        inline TReference At(size_t n) {
            if (n >= this->Size()) {
                ThrowRangeError("array ref range error");
            }

            return (*this)[n];
        }

        inline T& operator[](size_t n) noexcept {
            Y_ASSERT(n < this->Size());

            return *(this->Begin() + n);
        }

        //compat, do not use
        using iterator = TIterator;
        using reference = TReference;

        using TBase::at;
        using TBase::back;
        using TBase::begin;
        using TBase::end;
        using TBase::front;

        inline iterator begin() noexcept {
            return this->Begin();
        }

        inline iterator end() noexcept {
            return this->End();
        }

        inline reference front() noexcept {
            return this->Front();
        }

        inline reference back() noexcept {
            return this->Back();
        }

        inline reference at(size_t n) {
            return this->At(n);
        }
    };

    template <class T, class TVec>
    class TVectorOps<const T, TVec>: public TVectorOpsBase<const T, TVec> {
    };
}
