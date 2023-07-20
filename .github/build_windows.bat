call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
mkdir build
cd build
cmd.exe /c cmake -G "NMake Makefiles" ..
cmake -G "NMake Makefiles" -DBUILD_TEST=TRUE -DLWS_HAVE_PTHREAD_H=1 -DLWS_EXT_PTHREAD_INCLUDE_DIR="C:\\tools\\pthreads-w32-2-9-1-release\\Pre-built.2\\include" -DLWS_EXT_PTHREAD_LIBRARIES="C:\\tools\\pthreads-w32-2-9-1-release\\Pre-built.2\\lib\\x64\\libpthreadGC2.a" -DLWS_WITH_MINIMAL_EXAMPLES=1 -DLWS_OPENSSL_INCLUDE_DIRS="C:\\Program Files\\OpenSSL\\include" -DLWS_OPENSSL_LIBRARIES="C:\\Program Files\\OpenSSL\\lib\\libssl.lib;C:\\Program Files\\OpenSSL\\lib\\libcrypto.lib" ..
nmake