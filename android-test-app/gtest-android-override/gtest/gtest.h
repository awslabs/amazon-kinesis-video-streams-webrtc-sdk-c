// Wrapper that includes the real gtest.h and then disables death test macros.
// gtest 1.12.1 unconditionally defines GTEST_HAS_DEATH_TEST=1 on Linux/Android
// (no #ifndef guard in gtest-port.h).  Death tests use fork() which does not
// work inside an Android JNI process — the abort() kills the whole JVM instead
// of a forked child.  Override EXPECT_DEATH / ASSERT_DEATH to no-ops after the
// real header has been fully processed.

#pragma once

// include_next skips this directory and finds the real gtest/gtest.h
#pragma GCC system_header
#include_next "gtest/gtest.h"

#undef EXPECT_DEATH
#define EXPECT_DEATH(statement, regex) \
    GTEST_LOG_(WARNING) << "EXPECT_DEATH disabled (Android JNI): " #statement

#undef ASSERT_DEATH
#define ASSERT_DEATH(statement, regex) \
    GTEST_LOG_(WARNING) << "ASSERT_DEATH disabled (Android JNI): " #statement
