#pragma once

#include "fwd.h"
#include "reserve.h"

#include <util/memory/alloc.h>

#include <vector>
#include <initializer_list>

#if !defined(VECTOR_TLS)
# include <util/system/tls.h>
# define VECTOR_TLS
#endif

template <class T>
struct TVectorTraits {
    using Type = typename std::remove_cv<T>::type;
};

template <>
struct TVectorTraits<bool> {
    struct Type { // proxy-type to avoid vector<bool> specialization
        inline Type(): value(false) {}
        inline Type(bool v): value(v) {}
        inline operator bool&() noexcept { return value; }
        inline operator const bool&() const noexcept { return value; }
        inline bool* operator&() noexcept { return &value; }
        inline const bool* operator&() const noexcept { return &value; }
        inline Type& operator=(bool v) noexcept { value = v; return *this; }
        bool value;
    };
};

template <class T, class A>
class TVector: public std::vector<typename TVectorTraits<T>::Type,
    TReboundAllocator<A, typename TVectorTraits<T>::Type>>
{
public:
    using TValue = typename TVectorTraits<T>::Type;
    using TBase = std::vector<TValue, TReboundAllocator<A, TValue>>;
    using TSelf = TVector<T, A>;
    using size_type = typename TBase::size_type;

#if defined(VECTOR_TLS)
    inline static const TVector& Zeros(size_type count)
    {
        Y_STATIC_THREAD(TVector) zerosLocal; // TVector is non-POD
        TVector& result = TlsRef(zerosLocal);
        result.yresize(count);
        return result;
    }
#else
    inline static TVector Zeros(size_type count)
    {
        return TVector(count);
    }
#endif

    inline TVector()
        : TBase()
    {
    }

    inline TVector(const typename TBase::allocator_type& a)
        : TBase(a)
    {
    }

    inline explicit TVector(::NDetail::TReserveTag rt)
        : TBase()
    {
        this->reserve(rt.Capacity);
    }

    inline explicit TVector(::NDetail::TReserveTag rt, const typename TBase::allocator_type& a)
        : TBase(a)
    {
        this->reserve(rt.Capacity);
    }

    inline explicit TVector(size_type count)
        : TBase(count)
    {
    }

    inline TVector(size_type count, const TValue& val)
        : TBase(count, val)
    {
    }

    inline TVector(size_type count, const TValue& val, const typename TBase::allocator_type& a)
        : TBase(count, val, a)
    {
    }

    inline TVector(std::initializer_list<TValue> il)
        : TBase(il)
    {
    }

    inline TVector(std::initializer_list<TValue> il, const typename TBase::allocator_type& a)
        : TBase(il, a)
    {
    }

    inline TVector(const TSelf& src)
        : TBase(src)
    {
    }

    inline TVector(TSelf&& src) noexcept
        : TBase(std::forward<TSelf>(src))
    {
    }

    template <class TIter>
    inline TVector(TIter first, TIter last)
        : TBase(first, last)
    {
    }

    inline TSelf& operator=(const TSelf& src) {
        TBase::operator=(src);
        return *this;
    }

    inline TSelf& operator=(TSelf&& src) noexcept {
        TBase::operator=(std::forward<TSelf>(src));
        return *this;
    }

    inline TSelf& operator=(std::initializer_list<TValue> il) {
        this->assign(il.begin(), il.end());
        return *this;
    }

    inline explicit operator bool() const noexcept {
        return !this->empty();
    }

    Y_PURE_FUNCTION
    inline bool empty() const noexcept {
        return TBase::empty();
    }

    inline yssize_t ysize() const noexcept {
        return (yssize_t)TBase::size();
    }

    inline void yresize(size_type size) {
        TBase::resize(size);
    }

    inline void crop(size_type size) {
        if (this->size() > size) {
            this->erase(this->begin() + size, this->end());
        }
    }
};
