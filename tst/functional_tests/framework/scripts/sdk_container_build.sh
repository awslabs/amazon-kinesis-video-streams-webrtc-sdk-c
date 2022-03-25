#!/bin/bash

sdk_repository_path=$1
marker_file_path="/tmp/test-logs"

echo "** Building SDK **"

echo "Sdk repository path: ${sdk_repository_path}"
echo "Build logs path: ${marker_file_path}"

cd $sdk_repository_path
#rm -rf build
#rm -rf opensource

mkdir build
cd build

cmake .. && make
build_status_code=$?

echo "*** SDK BUILD STATUS CODE = ${build_status_code}"
if [ $build_status_code -ne 0 ]
then
    touch "${marker_file_path}/sdk_build_failed_marker_file"
else
    touch "${marker_file_path}/sdk_build_succeeded_marker_file"
fi

exit build_status_code