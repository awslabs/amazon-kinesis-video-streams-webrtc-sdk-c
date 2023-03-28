call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
mkdir build
cd build
cmd.exe /c cmake -G "NMake Makefiles" ..
cmake -G "NMake Makefiles" -DBUILD_TEST=TRUE -DLWS_WITH_THREADPOOL=ON -DLWS_EXT_PTHREAD_INCLUDE_DIR="C:\tools\pthreads-w32-2-9-1-release\Pre-built.2\include" -DLWS_EXT_PTHREAD_LIBRARIES="C:\tools\pthreads-w32-2-9-1-release\Pre-built.2\lib\x86\libpthreadGC2.a" -DLWS_WITH_MINIMAL_EXAMPLES=1 ..
nmake