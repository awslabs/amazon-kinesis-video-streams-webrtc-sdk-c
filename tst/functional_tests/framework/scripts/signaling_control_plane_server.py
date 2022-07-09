from flask import Flask, request, json, make_response
from channel import channel
from channel import metrics
import sys
import signal
import logging
import os

app = Flask(__name__)

channelMap = {}
channelArnMap = {}

metrics_file = ''
metricsMap = {}

def setupMetrics():
    global metrics_file
    metrics_file = os.environ['SIGNALING_CP_SERVER_METRICS_FILE']

def flush_metrics():
    app.logger.debug("Writing metrics to: " + metrics_file)
    metric_file = open(metrics_file, "w")
    for key in metricsMap:
        metricEntry = key + "=" + str(metricsMap[key])
        metric_file.write(metricEntry)
    metric_file.close()

@app.route('/bootstrap')
def bootstrap():
    setupMetrics()
    app.logger.debug("Loaded metrics file location : " + metrics_file)
    return "Successfully bootstrapped the server"

@app.route('/test_api')
def test_api():
    incrementMetricCount("test_api")
    return "called test_api"

@app.route('/shutdown')
def shutdown():
    flush_metrics()
    func = request.environ.get('werkzeug.server.shutdown')
    if func is None:
        raise RuntimeError('Not running with the Werkzeug Server')
    func()
    return "Shutting down the server"

def addMetric(metricName, count):
    metricsMap[metricName] = metricsMap[metricName] + count if metricName in metricsMap else count

def incrementMetricCount(metricName):
    addMetric(metricName, 1)
## ===================================================================== ##
# ===================== Describe Signaling Channel ====================== #
## ===================================================================== ##

def handleDescribeSignalingChannelRequest(channelName, channelArn):
    if channelName not in channelMap:
        responseMessage = {}
        responseMessage['Message'] = 'The requested channel is not found or not active.'
        incrementMetricCount(metrics.DESCRIBE_SIGNALING_CHANNEL_ENDPOINT_API_CALL_FAILURE)
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 404,
            mimetype='application/json'
        )

    ch = channelMap[channelName]
    if (channelArn != '' and channelArn != ch.channelArn):
        responseMessage = {}
        responseMessage['Message'] = f'Channel name/arn mismatch name = {channelName}, expected arn = {channelArn}, actual arn = {ch.channelArn}'
        incrementMetricCount(metrics.DESCRIBE_SIGNALING_CHANNEL_ENDPOINT_API_CALL_FAILURE)
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 400,
            mimetype='application/json'
        )

    content = {}
    content['ChannelInfo'] = ch.getChannelInfo()
    incrementMetricCount(metrics.DESCRIBE_SIGNALING_CHANNEL_ENDPOINT_API_CALL_SUCCESS)
    return app.response_class(
        response = json.dumps(content),
        status = 200,
        mimetype='application/json'
    )

@app.route("/describeSignalingChannel", methods = ['POST'])
def describeSignalingChannelHandler():
    requestPayload = request.json

    channelArn = ''
    if 'ChannelARN' in requestPayload:
        channelArn = requestPayload['ChannelARN']

    channelName = ''
    if 'ChannelName' in requestPayload:
        channelName = requestPayload['ChannelName']

    if channelName == '' and channelArn == '':
        responseMessage = {}
        responseMessage['Message'] = 'Bad request. Channel Name and Arn Parameter is missing/empty'
        incrementMetricCount(metrics.DESCRIBE_SIGNALING_CHANNEL_ENDPOINT_API_CALL_FAILURE)
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 400,
            mimetype='application/json'
        )
    
    channelName = requestPayload['ChannelName']

    return handleDescribeSignalingChannelRequest(channelName, channelArn)
    
    hannelRequest(channelName, channelArn)

## ===================================================================== ##
# ===================== Create Signaling Channel ====================== #
## ===================================================================== ##

def handleCreateSignalingChannelRequest(channelName):
    newChannel = channel.SignalingChanel(channelName, channel.SignalingChannelStatus.ACTIVE, 
        channel.SignalingChanelType.SINGLE_MASTER,60)
    channelArnMap[newChannel.channelArn] = channelName
    channelMap[channelName] = newChannel
    content = {}
    content['ChannelARN'] = newChannel.channelArn

    incrementMetricCount(metrics.CREATE_SIGNALING_CHANNEL_ENDPOINT_API_CALL_SUCCESS)
    return app.response_class(
        response = json.dumps(content),
        status = 200,
        mimetype='application/json'
    )

@app.route("/createSignalingChannel", methods = ['POST'])
def createSignalingChannelHandler():
    requestPayload = request.json

    if 'ChannelName' not in requestPayload or requestPayload['ChannelName'] == '':
        responseMessage = {}
        responseMessage['Message'] = 'Bad request. Channel Name Parameter is missing/empty'
        incrementMetricCount(metrics.CREATE_SIGNALING_CHANNEL_ENDPOINT_API_CALL_FAILURE)
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 400,
            mimetype='application/json'
        )
    
    channelName = requestPayload['ChannelName']

    return handleCreateSignalingChannelRequest(channelName)

## ===================================================================== ##
# ===================== Get Signaling Channel Endpoint ================== #
## ===================================================================== ##

def handleGetSignalingChannelEndpointRequest():
    resource = {}
    resource['Protocol'] = 'HTTPS'
    resource['ResourceEndpoint'] = 'https://kvs-sdk-test-signaling-server.com'

    resources = []
    resources.append(resource)
    resourceEndPointList = {}
    resourceEndPointList['ResourceEndpointList'] = resources

    incrementMetricCount(metrics.GET_SIGNALING_CHANNEL_ENDPOINT_API_CALL_SUCCESS)
    return app.response_class(
        response = json.dumps(resourceEndPointList),
        status = 200,
        mimetype='application/json'
    )

@app.route("/getSignalingChannelEndpoint", methods = ['POST'])
def getSignalingChannelEndpointHandler():
    data = request.json
    channelArn = data['ChannelARN']
    if channelArn not in channelArnMap:
        responseMessage = {}
        responseMessage['Message'] = f'Bad request. Invalid channel Arn {channelArn}'
        incrementMetricCount(metrics.GET_SIGNALING_CHANNEL_ENDPOINT_API_CALL_FAILURE)
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 400,
            mimetype='application/json'
        )

    return handleGetSignalingChannelEndpointRequest()

## ===================================================================== ##
# ===================== Get Ice Server Config =========================== #
## ===================================================================== ##

def handleGetIceServerConfigRequest():
    iceServerConfig1 = {}
    iceServerConfig1['Password'] = 'HTTPS'
    iceServerConfig1['Ttl'] = '86400'
    uris1 = []
    # uris1.append('turn:18-236-204-50.t-13dca313.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp')
    # uris1.append('turns:18-236-204-50.t-13dca313.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp')
    uris1.append('turns:18-236-204-50.t-13dca313.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp')
    iceServerConfig1['Uris'] = uris1
    iceServerConfig1['Username'] = '1641542855:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjMwMjMxOTI5ODAwNjpjaGFubmVsL2FrYXRleS10ZXN0LWNoYW5uZWwtMTIvMTY0MTU0MjU1NTUwNw=='

    iceConfigs = []
    iceConfigs.append(iceServerConfig1)

    iceServerList = {}
    iceServerList['IceServerList'] = iceConfigs

    incrementMetricCount(metrics.GET_ICE_SERVER_CONFIG_API_CALL_SUCCESS)
    return app.response_class(
        response = json.dumps(iceServerList),
        status = 200,
        mimetype='application/json'
    )

@app.route("/v1/get-ice-server-config", methods = ['POST'])
def getIceServerConfigHandler():
    data = request.json
    channelArn = data['ChannelARN']
    if channelArn not in channelArnMap:
        responseMessage = {}
        responseMessage['Message'] = f'Bad request. Invalid channel Arn {channelArn}'
        incrementMetricCount(metrics.GET_ICE_SERVER_CONFIG_API_CALL_FAILURE)
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 400,
            mimetype='application/json'
        )

    return handleGetIceServerConfigRequest()