package com.kvs.webrtctest;

public class NativeTestLib {
    static {
        System.loadLibrary("webrtc_test_jni");
    }

    /**
     * Run native gtest tests.
     *
     * @param workDir working directory (tests expect ../samples/ relative to this)
     * @param filter  gtest filter expression (e.g. "StunApiTest.*"), "*" for all
     * @return 0 on success, nonzero on failure
     */
    public static native int runTests(String workDir, String filter);
}
