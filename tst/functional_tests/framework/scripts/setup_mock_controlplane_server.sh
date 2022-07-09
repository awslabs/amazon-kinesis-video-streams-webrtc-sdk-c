#!/bin/bash

echo "** Starting Signaling CP server **"

server_directory=$1
logging_directory=$2
server_ip_address=$3
server_port=$4

rm "${logging_directory}/signaling_cp_server_log"
rm "${logging_directory}/signaling_cp_server_metrics"

touch "${logging_directory}/signaling_cp_server_log"
touch "${logging_directory}/signaling_cp_server_metrics"
export SIGNALING_CP_SERVER_LOG_FILE="${logging_directory}/signaling_cp_server_log"
export SIGNALING_CP_SERVER_METRICS_FILE="${logging_directory}/signaling_cp_server_metrics"

cd $server_directory

export SIGNALING_CP_SERVER_LOG_FILE="/tmp/signaling_cp_server_log"
export SIGNALING_CP_SERVER_METRICS_FILE="/tmp/signaling_cp_server_metrics"

# export server_ip_address=172.22.0.12
# export server_port=5443

export FLASK_APP=signaling_control_plane_server
export FLASK_ENV=development
export FLASK_DEBUG=1
export SERVER_IP_ADDRESS=$server_ip_address #172.22.0.12
export SERVER_PORT=$server_port #5443

echo "Starting Signaling CP server - $SERVER_IP_ADDRESS:$SERVER_PORT"

nohup flask run --host=$SERVER_IP_ADDRESS --port=$SERVER_PORT > SIGNALING_CP_SERVER_LOG_FILE 2>&1 &
sleep 3
curl http://$SERVER_IP_ADDRESS:$SERVER_PORT/bootstrap



