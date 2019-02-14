#pragma once

#include <util/system/yassert.h>
#include <util/generic/algorithm.h>

#ifdef _DEBUG
template <class T>
struct TBoundCheck {
    T* Data;
    size_t Size;
    TBoundCheck(T* d, size_t s) {
        Data = d;
        Size = s;
    }
    T& operator[](size_t i) const {
        Y_ASSERT(i >= 0 && i < Size);
        return Data[i];
    }
};
#endif

template <class T>
class TArray2D {
private:
    typedef T* PT;
    T* Data;
    PT* PData;
    size_t XSize;
    size_t YSize;
    size_t Capacity;

public:
    TArray2D(size_t xsize = 1, size_t ysize = 1)
        : XSize(xsize), YSize(ysize), Capacity(xsize * ysize)
        , Data(0 < Capacity ? new T[Capacity] : NULL)
        , PData(0 < Capacity ? new PT[ysize] : NULL)
    {
        T* data = Data;
        for (size_t i = 0; i < YSize; ++i) {
            PData[i] = data;
            data += XSize;
        }
    }
    TArray2D(const TArray2D& a)
        : XSize(a.XSize), YSize(a.YSize), Capacity(a.XSize * a.YSize)
        , Data(0 < Capacity ? new T[Capacity] : NULL)
        , PData(0 < Capacity ? new PT[a.YSize] : NULL)
    {
        size_t k = 0;
        for (size_t j = 0; j < YSize; ++j) {
            const size_t kNext = k + XSize;
            for (size_t i = k; i < kNext; ++i) {
                Data[i] = a.Data[i];
            }
            PData[j] = Data + k;
            k = kNext;
        }
    }
    TArray2D& operator=(const TArray2D& a) {
        const size_t size = a.XSize * a.YSize;
        SetSizes(a.XSize, a.YSize)();
        memcpy(Data, a.Data, sizeof(T) * size);
        return *this;
    }
    ~TArray2D() {
        delete[] Data;
        delete[] PData;
    }
    void SetSizes(size_t xsize, size_t ysize) {
        if (XSize != xsize || YSize != ysize) {
            const size_t size = xsize * ysize;
            if (size <= Capacity) { // update
                XSize = xsize;
                YSize = ysize;
            }
            else { // grow
                TArray2D(xsize, ysize).Swap(*this);
            }
        }
    }
    void Clear() {
        SetSizes(1, 1);
        Data[0] = 0;
    }
#ifdef _DEBUG
    TBoundCheck<T> operator[](size_t i) const {
        Y_ASSERT(i >= 0 && i < YSize);
        return TBoundCheck<T>(PData[i], XSize);
    }
#else
    T* operator[](size_t i) const {
        Y_ASSERT(i >= 0 && i < YSize);
        return PData[i];
    }
#endif
    size_t GetXSize() const {
        return XSize;
    }
    size_t GetYSize() const {
        return YSize;
    }
    void FillZero() {
        memset(Data, 0, sizeof(T) * XSize * YSize);
    }
    void FillEvery(const T& a) {
        std::fill(Data, Data + XSize * YSize, a);
    }
    void Swap(TArray2D& a) {
        std::swap(Data, a.Data);
        std::swap(PData, a.PData);
        std::swap(XSize, a.XSize);
        std::swap(YSize, a.YSize);
        std::swap(Capacity, a.Capacity);
    }
};

template <class T>
inline bool operator==(const TArray2D<T>& a, const TArray2D<T>& b) {
    if (a.GetXSize() != b.GetXSize() || a.GetYSize() != b.GetYSize()) {
        return false;
    }
    for (size_t y = 0; y < a.GetYSize(); ++y) {
        for (size_t x = 0; x < a.GetXSize(); ++x) {
            if (a[y][x] != b[y][x]) {
                return false;
            }
        }
    }
    return true;
}

template <class T>
inline bool operator!=(const TArray2D<T>& a, const TArray2D<T>& b) {
    return !(a == b);
}
