#include <library/threading/name_guard/name_guard.h>
#include <library/unittest/registar.h>

#include <util/system/thread.h>
#include <util/thread/factory.h>

Y_UNIT_TEST_SUITE(ThreadNameGuardTests) {
    Y_UNIT_TEST(Test) {
        const TString nameBefore = "nameBefore";
        const TString nameToSet = "nameToSet";
        SystemThreadPool()->Run([&] {
            TThread::CurrentThreadSetName(nameBefore.c_str());

            {
                Y_THREAD_NAME_GUARD(nameToSet);
                const auto name = TThread::CurrentThreadGetName();

                UNIT_ASSERT_VALUES_EQUAL(nameToSet, name);
            }

            const auto name = TThread::CurrentThreadGetName();
            UNIT_ASSERT_VALUES_EQUAL(nameBefore, name);
        })->Join();
    }
}
