#pragma once

/// This code should not be used directly unless you really understand what you do.
/// If you need threads, use thread pool functionality in <util/thread/factory.h>
/// @see SystemThreadPool()

#include <util/generic/ptr.h>
#include <util/generic/string.h>

#include "defaults.h"
#include "progname.h"

bool SetHighestThreadPriority();

class TThread {
public:
    using TThreadProc = void* (*)(void*);
    using TId = size_t;

    struct TParams {
        TThreadProc Proc;
        void* Data;
        size_t StackSize;
        void* StackPointer;
        // See comments for `CurrentThreadSetName`
        TString Name = GetProgramName();

        inline TParams()
            : Proc(nullptr)
            , Data(nullptr)
            , StackSize(0)
            , StackPointer(nullptr)
        {
        }

        inline TParams(TThreadProc proc, void* data)
            : Proc(proc)
            , Data(data)
            , StackSize(0)
            , StackPointer(nullptr)
        {
        }

        inline TParams(TThreadProc proc, void* data, size_t stackSize)
            : Proc(proc)
            , Data(data)
            , StackSize(stackSize)
            , StackPointer(nullptr)
        {
        }

        inline TParams& SetName(const TString& name) noexcept {
            Name = name;

            return *this;
        }

        inline TParams& SetStackSize(size_t size) noexcept {
            StackSize = size;

            return *this;
        }

        inline TParams& SetStackPointer(void* ptr) noexcept {
            StackPointer = ptr;

            return *this;
        }
    };

    TThread(const TParams& params);
    TThread(TThreadProc threadProc, void* param);

    ~TThread();

    void Start();

    void* Join();
    void Detach();
    bool Running() const noexcept;
    TId Id() const noexcept;

    static TId ImpossibleThreadId() noexcept;
    static TId CurrentThreadId() noexcept;

    // NOTE: Content of `name` will be copied.
    //
    // NOTE: On Linux thread name is limited to 15 symbols which is probably the smallest one among
    // all platforms. If you'll provide name longer than 15 symbols it will be cut. So if you expect
    // `CurrentThreadGetName` to return the same name as `name` make sure it's not longer than 15
    // symbols.
    static void CurrentThreadSetName(const char* name);

    // NOTE: May return empty string for platforms different from Darwin or Linux.
    static TString CurrentThreadGetName();

private:
    class TImpl;
    THolder<TImpl> Impl_;
};

class ISimpleThread: public TThread {
public:
    ISimpleThread(size_t stackSize = 0);

    virtual ~ISimpleThread() = default;

    virtual void* ThreadProc() = 0;
};

struct TCurrentThreadLimits {
    TCurrentThreadLimits() noexcept;

    const void* StackBegin;
    size_t StackLength;
};
