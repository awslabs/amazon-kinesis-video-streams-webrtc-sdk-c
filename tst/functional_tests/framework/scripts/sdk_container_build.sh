#!/bin/bash

sdk_repository_path=$1
sdk_build_log=$2
$clean_build_option=$3

echo "** Building SDK **"

echo "Sdk repository path: ${sdk_repository_path}"
echo "Build logs path: ${sdk_build_log}"

cd $sdk_repository_path

if [ -z "$clean_build_option" ]
then
  echo "Cleaning build and open-source directory"
  rm -rf build
  rm -rf open-source
else
  echo "Sipping cleaning build/open source directory before build"
fi

mkdir build
cd build

cmake .. -DBUILD_TEST=TRUE && make > $sdk_build_log 2>&1 &
build_status_code=$?

echo "SDK BUILD STATUS CODE = ${build_status_code}"
exit $build_status_code