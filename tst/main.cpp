/**
 * main class that contains only main method to trigger all tests
 */

#include "gtest/gtest.h"
#include <unordered_set>
#include <string>
#include <sstream>

#ifdef ENABLE_AWS_SDK_IN_TESTS
#include <aws/core/Aws.h>
#endif

// The number of retries allowed. 0 means no retry, all tests will run exactly run once.
#define MAX_TRIALS 10

using namespace std;

class Retrier : public ::testing::EmptyTestEventListener {
  public:
    string testFilter()
    {
        stringstream ss;
        for (const string& testPath : failedTests) {
            ss << ":" << testPath;
        }
        return ss.str();
    }

  protected:
    virtual void OnTestEnd(const ::testing::TestInfo& info)
    {
        // easy way to convert c string to c++ string
        string testCaseName = info.test_case_name();
        string name = info.name();
        string testPath = testCaseName + "." + name;

        if (info.result()->Passed()) {
            failedTests.erase(testPath);
        } else {
            failedTests.insert(testPath);
        }
    }

  private:
    unordered_set<string> failedTests;
};

int main(int argc, char** argv)
{
    int trial = 0, rc;
    bool breakOnFailure;
#ifdef ENABLE_AWS_SDK_IN_TESTS
    Aws::SDKOptions options;
#endif
    ::testing::InitGoogleTest(&argc, argv);
    breakOnFailure = ::testing::GTEST_FLAG(break_on_failure);

#ifdef ENABLE_AWS_SDK_IN_TESTS
    Aws::InitAPI(options);
#endif

    Retrier* retrier = new Retrier();
    // Adds a listener to the end. googletest takes the ownership.
    ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(retrier);

    // Temporarily turn off the break_on_failure flag until the last trial. Otherwise, the retrier won't
    // be able to retry the failed tests since googletest will forcefully quit.
    ::testing::GTEST_FLAG(break_on_failure) = false;

    do {
        // Since this is the last trial, break_on_failure flag should be turned back on
        // if it was specified.
        if (trial >= MAX_TRIALS) {
            ::testing::GTEST_FLAG(break_on_failure) = breakOnFailure;
        }
        rc = RUN_ALL_TESTS();

        // If there were some tests failed, set googletest filter flag to those failed tests.
        // If no test failed, the flag should be set to empty and RUN_ALL_TESTS should not be called
        // again since it should break out from the loop.
        ::testing::GTEST_FLAG(filter) = retrier->testFilter();
    } while (rc != 0 && trial++ < MAX_TRIALS);

#ifdef ENABLE_AWS_SDK_IN_TESTS
    Aws::ShutdownAPI(options);
#endif

    return rc;
}
