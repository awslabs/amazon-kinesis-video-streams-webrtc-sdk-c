call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
mkdir build
cd build
cmd.exe /c cmake -G "NMake Makefiles" ..
cmake -G "NMake Makefiles" -DBUILD_TEST=TRUE -DLWS_HAVE_PTHREAD_H=1 -DLWS_EXT_PTHREAD_INCLUDE_DIR="C:\tools\pthreads-w64-2-10-0-release\Pre-built.2\include" -DLWS_EXT_PTHREAD_LIBRARIES="C:\tools\pthreads-w64-2-10-0-release\Pre-built.2\lib\x64\libpthreadGC2.a" -DLWS_WITH_MINIMAL_EXAMPLES=1 ..
nmake