#!/bin/bash


container_subnet=$1
master_container_name=$2
viewer_container_name=$3
mount_directory1=$1
mount_directory2=$2


docker network create --subnet=172.18.0.0/16 webrtc_peer_network


--ip 172.18.0.22



docker run -t -d -P -v $mount_directory1 -v $mount_directory2 --ip 172.18.0.22


    string command = "docker run -t -d -P ";
    // docker run -t -d -P -v ~/kvs_git_workspace:/tmp/xyz --name viewer-alpine webrtc-alpine-image
    command += " -v " + functionalTestConfig.sourceRepoPath + ":" + "/tmp/test-sdk-source/";
    command += " -v " + functionalTestConfig.testArtifactsLoggingDirectory + ":" + "/tmp/test-logs/";
    command += " --name " + functionalTestConfig.masterContainerName;
    command += " " + functionalTestConfig.containerImageName;