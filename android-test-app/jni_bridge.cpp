#include <jni.h>
#include <unistd.h>
#include <string>
#include <android/log.h>

#include "gtest/gtest.h"
#include "com/amazonaws/kinesis/video/common/CommonDefs.h"
#include "com/amazonaws/kinesis/video/common/PlatformUtils.h"

#define LOG_TAG "webrtc_test_jni"

// Custom gtest listener that routes output to Android logcat.
class LogcatPrinter : public ::testing::EmptyTestEventListener {
  public:
    void OnTestProgramStart(const ::testing::UnitTest& unit_test) override
    {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[==========] Running %d tests from %d test suites.",
                            unit_test.test_to_run_count(), unit_test.test_suite_to_run_count());
    }

    void OnTestSuiteStart(const ::testing::TestSuite& suite) override
    {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[----------] %d tests from %s", suite.test_to_run_count(), suite.name());
    }

    void OnTestStart(const ::testing::TestInfo& info) override
    {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[ RUN      ] %s.%s", info.test_suite_name(), info.name());
    }

    void OnTestPartResult(const ::testing::TestPartResult& result) override
    {
        if (result.failed()) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s:%d: Failure\n%s",
                                result.file_name() ? result.file_name() : "unknown", result.line_number(),
                                result.message() ? result.message() : "");
        }
    }

    void OnTestEnd(const ::testing::TestInfo& info) override
    {
        if (info.result()->Passed()) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[       OK ] %s.%s (%lld ms)", info.test_suite_name(), info.name(),
                                (long long) info.result()->elapsed_time());
        } else {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "[  FAILED  ] %s.%s (%lld ms)", info.test_suite_name(), info.name(),
                                (long long) info.result()->elapsed_time());
        }
    }

    void OnTestSuiteEnd(const ::testing::TestSuite& suite) override
    {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[----------] %d tests from %s (%lld ms total)", suite.test_to_run_count(),
                            suite.name(), (long long) suite.elapsed_time());
    }

    void OnTestProgramEnd(const ::testing::UnitTest& unit_test) override
    {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[==========] %d tests from %d test suites ran. (%lld ms total)",
                            unit_test.test_to_run_count(), unit_test.test_suite_to_run_count(),
                            (long long) unit_test.elapsed_time());
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[  PASSED  ] %d tests.", unit_test.successful_test_count());
        if (unit_test.failed_test_count() > 0) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "[  FAILED  ] %d tests.", unit_test.failed_test_count());
        }
    }
};

// Logcat-backed log function matching kvspic's logPrintFunc signature.
static VOID logcatLogPrint(UINT32 level, const PCHAR tag, const PCHAR fmt, ...)
{
    int androidLevel;
    switch (level) {
        case LOG_LEVEL_VERBOSE:
            // androidLevel = ANDROID_LOG_VERBOSE;
            // break;
            // too much verbose logs
            return;
        case LOG_LEVEL_DEBUG:
            androidLevel = ANDROID_LOG_DEBUG;
            break;
        case LOG_LEVEL_INFO:
            androidLevel = ANDROID_LOG_INFO;
            break;
        case LOG_LEVEL_WARN:
            androidLevel = ANDROID_LOG_WARN;
            break;
        case LOG_LEVEL_ERROR:
            androidLevel = ANDROID_LOG_ERROR;
            break;
        case LOG_LEVEL_FATAL:
            androidLevel = ANDROID_LOG_FATAL;
            break;
        default:
            androidLevel = ANDROID_LOG_DEFAULT;
            break;
    }
    va_list args;
    va_start(args, fmt);
    if (tag) {
        char taggedFmt[1024];
        snprintf(taggedFmt, sizeof(taggedFmt), "[%s] %s", tag, fmt);
        __android_log_vprint(androidLevel, LOG_TAG, taggedFmt, args);
    } else {
        __android_log_vprint(androidLevel, LOG_TAG, fmt, args);
    }
    va_end(args);
}

extern "C" JNIEXPORT jint JNICALL Java_com_kvs_webrtctest_NativeTestLib_runTests(JNIEnv* env, jclass /* clazz */, jstring workDir,
                                                                                  jstring filter)
{
    // Route KVS SDK logs to logcat
    globalCustomLogPrintFn = logcatLogPrint;

    const char* workDirStr = env->GetStringUTFChars(workDir, nullptr);
    const char* filterStr = env->GetStringUTFChars(filter, nullptr);

    // Change to working directory so tests find sample data at ../samples/
    chdir(workDirStr);

    // Build gtest argv
    std::string filterArg = std::string("--gtest_filter=") + filterStr;
    char arg0[] = "webrtc_test_jni";
    char* argv[] = {arg0, const_cast<char*>(filterArg.c_str()), "--gtest_fail_fast", nullptr};
    int argc = 3;

    ::testing::InitGoogleTest(&argc, argv);

    // Replace default stdout printer with logcat printer
    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
    delete listeners.Release(listeners.default_result_printer());
    listeners.Append(new LogcatPrinter());

    int rc = RUN_ALL_TESTS();

    env->ReleaseStringUTFChars(filter, filterStr);
    env->ReleaseStringUTFChars(workDir, workDirStr);

    return rc;
}
