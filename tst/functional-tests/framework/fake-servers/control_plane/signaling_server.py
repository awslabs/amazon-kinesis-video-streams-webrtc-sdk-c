from flask import Flask, request, json, make_response
from channel import channel

app = Flask(__name__)

channelMap = {}
channelArnMap = {}

@app.route("/abc", methods = ['GET'])
def hello_world():
    return "<p>Hello, World!</p>"

@app.route("/test_post", methods = ['POST'])
def hellow_world_post():
    data = json.loads(request.data)
    user = data['name']
    return f'<p>Hello, World! {user} </p>'

## ===================================================================== ##
# ===================== Describe Signaling Channel ====================== #
## ===================================================================== ##

def handleDescribeSignalingChannelRequest(channelName, channelArn):
    if channelName not in channelMap:
        responseMessage = {}
        responseMessage['Message'] = 'The requested channel is not found or not active.'
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 404,
            mimetype='application/json'
        )

    ch = channelMap[channelName]
    if (channelArn != '' and channelArn != ch.channelArn):
        responseMessage = {}
        responseMessage['Message'] = f'Channel name/arn mismatch name = {channelName}, expected arn = {channelArn}, actual arn = {ch.channelArn}'
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 400,
            mimetype='application/json'
        )

    content = {}
    content['ChannelInfo'] = ch.getChannelInfo()
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

def handleSignalingChannelEndpointRequest():
    resource = {}
    resource['Protocol'] = 'HTTPS'
    resource['ResourceEndpoint'] = 'https://kvs-sdk-test-signaling-server.com'

    resources = []
    resources.append(resource)
    resourceEndPointList = {}
    resourceEndPointList['ResourceEndpointList'] = resources

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
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 400,
            mimetype='application/json'
        )

    return handleSignalingChannelEndpointRequest()

## ===================================================================== ##
# ===================== Get Ice Server Config =========================== #
## ===================================================================== ##

def handleGetIceServerConfigRequest():
    iceServerConfig1 = {}
    iceServerConfig1['Password'] = 'HTTPS'
    iceServerConfig1['Ttl'] = '86400'
    uris1 = []
    uris1.append('turn:18-236-204-50.t-13dca313.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp')
    uris1.append('turns:18-236-204-50.t-13dca313.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp')
    uris1.append('turns:18-236-204-50.t-13dca313.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp')
    iceServerConfig1['Uris'] = uris1
    iceServerConfig1['Username'] = '1641542855:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjMwMjMxOTI5ODAwNjpjaGFubmVsL2FrYXRleS10ZXN0LWNoYW5uZWwtMTIvMTY0MTU0MjU1NTUwNw=='

    iceConfigs = []
    iceConfigs.append(iceServerConfig1)

    iceServerList = {}
    iceServerList['IceServerList'] = iceConfigs

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
        return app.response_class(
            response = json.dumps(responseMessage),
            status = 400,
            mimetype='application/json'
        )

    return handleGetIceServerConfigRequest()

if __name__ == 'main':
    #app.debug = True
    app.run(threaded=True, processes=20, host = '127.0.0.1', port=5443)