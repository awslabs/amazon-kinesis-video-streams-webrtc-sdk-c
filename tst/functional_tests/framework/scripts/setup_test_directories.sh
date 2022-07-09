#!/bin/bash

echo "** Setting up mock test directories **"

test_directory=$1
source_code_directory=$2

echo "Creating source code directory ${source_code_directory}"
mkdir -p source_code_directory

echo "Creating test root directory ${test_directory}"
mkdir -p $test_directory

mkdir -p $test_directory/marker_files
mkdir -p $test_directory/logs
touch $test_directory/logs/sdk-build-log