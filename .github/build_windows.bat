call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
mkdir build
cd build
cmd.exe /c cmake -G "MinGW Makefiles" ..
cmake -G "MinGW Makefiles" -DBUILD_TEST=TRUE ..
nmake