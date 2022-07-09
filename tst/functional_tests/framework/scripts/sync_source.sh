#!/bin/bash

sdk_working_directory=$1
repo_name=$2
container_sdk_source_directory=$3
skip_clean_build_option=$4

echo "##################### sdk_working_directory = ${sdk_working_directory}"
echo "##################### repo_name = ${repo_name}"
echo "##################### container_sdk_source_directory = ${container_sdk_source_directory}"

mkdir -p /tmp/temp-sdk-source
cp -r $sdk_working_directory /tmp/temp-sdk-source/ 2>/dev/null

#ls -l /tmp/temp-sdk-source/$repo_name


rm -rf /tmp/temp-sdk-source/$repo_name/build
rm -rf /tmp/temp-sdk-source/$repo_name/open-source


mkdir -p $container_sdk_source_directory
cp -r /tmp/temp-sdk-source/* $container_sdk_source_directory/ 2>/dev/null
rm -rf /tmp/temp-sdk-source