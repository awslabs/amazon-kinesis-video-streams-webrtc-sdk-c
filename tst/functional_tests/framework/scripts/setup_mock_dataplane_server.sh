#!/bin/bash

kill -9 $(pgrep -f signaling_websockets_server.py)

server_directory=$1
logging_directory=$2
server_ip_address=$3
server_port=$4

rm "${logging_directory}/signaling_dp_server_log"
rm "${logging_directory}/signaling_dp_server_metrics"

touch "${logging_directory}/signaling_dp_server_log"
touch "${logging_directory}/signaling_dp_server_metrics"
export SIGNALING_DP_SERVER_LOG_FILE="${logging_directory}/signaling_dp_server_log"
export SIGNALING_DP_SERVER_METRICS_FILE="${logging_directory}/signaling_dp_server_metrics"

cd $server_directory

export SIGNALING_DP_SERVER_LOG_FILE="/tmp/signaling_dp_server_log"
export SIGNALING_DP_SERVER_METRICS_FILE="/tmp/signaling_dp_server_metrics"
export SERVER_IP_ADDRESS=$server_ip_address #172.22.0.13
export SERVER_PORT=$server_port #8765

echo "Starting Signaling DP server - $server_ip_address:$server_port"

nohup python3 signaling_websockets_server.py > SIGNALING_DP_SERVER_LOG_FILE 2>&1 &
sleep 2