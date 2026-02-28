package com.kvs.webrtctest;

import android.content.Context;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

@RunWith(AndroidJUnit4.class)
public class WebRtcNativeTest {

    private static final String TAG = "WebRtcNativeTest";

    private static final String[] SAMPLE_DIRS = {
            "h264SampleFrames",
            "h265SampleFrames",
    };

    private String workDir;
    private String filter;

    @Before
    public void setUp() throws IOException {
        Context ctx = InstrumentationRegistry.getInstrumentation().getTargetContext();
        File filesDir = ctx.getFilesDir();

        // Extract sample assets to <filesDir>/samples/<dir>/
        File samplesDir = new File(filesDir, "samples");
        if (!samplesDir.exists()) {
            AssetManager assets = ctx.getAssets();
            for (String dir : SAMPLE_DIRS) {
                extractAssetDir(assets, dir, new File(samplesDir, dir));
            }
        }

        // Create tst/ work directory (tests run from here, expect ../samples/)
        File tstDir = new File(filesDir, "tst");
        tstDir.mkdirs();
        workDir = tstDir.getAbsolutePath();

        // Read gtest filter from instrumentation args (default: all tests)
        Bundle args = InstrumentationRegistry.getArguments();
        filter = args.getString("gtest_filter", "*");

        Log.i(TAG, "Work dir: " + workDir);
        Log.i(TAG, "Samples dir: " + samplesDir.getAbsolutePath());
        Log.i(TAG, "Filter: " + filter);
    }

    private static final long TIMEOUT_MINUTES = 5;

    @Test
    public void runNativeTests() throws Exception {
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Future<Integer> future = executor.submit(() -> NativeTestLib.runTests(workDir, filter));
        try {
            int rc = future.get(TIMEOUT_MINUTES, TimeUnit.MINUTES);
            assertEquals("Native gtest tests failed (see logcat tag 'webrtc_test_jni' for details)", 0, rc);
        } catch (TimeoutException e) {
            future.cancel(true);
            fail("Native tests timed out after " + TIMEOUT_MINUTES + " minutes (see logcat tag 'webrtc_test_jni' for details)");
        } finally {
            executor.shutdownNow();
        }
    }

    private void extractAssetDir(AssetManager assets, String assetPath, File destDir) throws IOException {
        String[] children = assets.list(assetPath);
        if (children == null || children.length == 0) {
            // Leaf file
            destDir.getParentFile().mkdirs();
            try (InputStream in = assets.open(assetPath);
                 OutputStream out = new FileOutputStream(destDir)) {
                byte[] buf = new byte[8192];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
            }
            return;
        }

        destDir.mkdirs();
        for (String child : children) {
            extractAssetDir(assets, assetPath + "/" + child, new File(destDir, child));
        }
    }
}
