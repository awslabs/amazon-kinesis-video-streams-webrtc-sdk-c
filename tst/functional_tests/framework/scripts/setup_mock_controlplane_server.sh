#!/bin/bash

echo "** Starting Signaling CP server **"

kill -9 $(pgrep -f flask)

logging_directory=$1
server_directory=$2

rm "${logging_directory}/signaling_cp_server_log"
rm "${logging_directory}/signaling_cp_server_metrics"

touch "${logging_directory}/signaling_cp_server_log"
touch "${logging_directory}/signaling_cp_server_metrics"
export SIGNALING_CP_SERVER_LOG_FILE="${logging_directory}/signaling_cp_server_log"
export SIGNALING_CP_SERVER_METRICS_FILE="${logging_directory}/signaling_cp_server_metrics"

cd $server_directory

export SIGNALING_CP_SERVER_LOG_FILE="/tmp/signaling_cp_server_log"
export SIGNALING_CP_SERVER_METRICS_FILE="/tmp/signaling_cp_server_metrics"

export FLASK_APP=signaling_control_plane_server
export FLASK_ENV=development
export FLASK_DEBUG=1

flask run --host=172.22.0.12 --port=5443 &

