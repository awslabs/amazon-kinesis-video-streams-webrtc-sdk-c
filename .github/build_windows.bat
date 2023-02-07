call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
mkdir build
cd build
cmd.exe /c cmake -G "NMake Makefiles" -DOPENSSL_ROOT_DIR=D:\a\amazon-kinesis-video-streams-webrtc-sdk-c\amazon-kinesis-video-streams-webrtc-sdk-c\open-source\libopenssl\ ..
cmake -G "NMake Makefiles" -DBUILD_TEST=TRUE -DOPENSSL_ROOT_DIR=D:\a\amazon-kinesis-video-streams-webrtc-sdk-c\amazon-kinesis-video-streams-webrtc-sdk-c\open-source\libopenssl\ ..
nmake