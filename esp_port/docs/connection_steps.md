# WebRTC SDK Connection Process

This document outlines the sequential steps that occur during the WebRTC connection process with Kinesis Video Streams, based on detailed logs from a successful connection.

## 1. Initial Setup
- Application initializes with channel name `demo-channel`
- Loads certificates from specified paths (`certificate.pem`, `private.key`, `cacert.pem`)

## 2. Credential Setup
- Creates IoT credential provider with endpoint: `c3mh3f5xs9l81c.credentials.iot.us-east-1.amazonaws.com`
- Uses role alias `webrtc_iot_role_alias` and thing name `webrtc_iot_thing`
- Fetches IoT credentials via HTTPS request to the credentials endpoint
  - Getting IoT credential with URL: `https://c3mh3f5xs9l81c.credentials.iot.us-east-1.amazonaws.com/role-aliases/webrtc_iot_role_alias/credentials`
- Receives credentials with expiration time (valid for 1 hour)

## 3. Media Source Initialization
- Discovers sample H.264 video frames from the samples directory
- Initializes WebRTC and SRTP security components

## 4. Signaling Client Creation
- Initializes signaling client in "New" state
- Transitions to "Get Security Credentials" state using the IoT credentials fetched earlier
- Control plane URL is generated: `https://kinesisvideo.us-east-1.amazonaws.com`

## 5. Channel Description
- Transitions to "Describe Channel" state
- Sends API request to: `https://kinesisvideo.us-east-1.amazonaws.com/describeSignalingChannel`
- Request body:
  ```json
  {
    "ChannelName": "demo-channel"
  }
  ```
- API Response:
  ```json
  {
    "ChannelInfo": {
      "ChannelARN": "arn:aws:kinesisvideo:us-east-1:995678934610:channel/demo-channel/1712136149494",
      "ChannelName": "demo-channel",
      "ChannelStatus": "ACTIVE",
      "ChannelType": "SINGLE_MASTER",
      "CreationTime": 1.712136149494E9,
      "FullMeshConfiguration": null,
      "SingleMasterConfiguration": {
        "MessageTtlSeconds": 60
      },
      "Version": "3Ikjs9sOSk6Q4Alisl0d"
    }
  }
  ```

## 6. Channel Endpoint Retrieval
- Transitions to "Get Channel Endpoint" state
- Sends API request to: `https://kinesisvideo.us-east-1.amazonaws.com/getSignalingChannelEndpoint`
- Request body:
  ```json
  {
    "ChannelARN": "arn:aws:kinesisvideo:us-east-1:995678934610:channel/demo-channel/1712136149494",
    "SingleMasterChannelEndpointConfiguration": {
      "Protocols": ["WSS", "HTTPS"],
      "Role": "MASTER"
    }
  }
  ```
- API Response:
  ```json
  {
    "ResourceEndpointList": [
      {
        "Protocol": "HTTPS",
        "ResourceEndpoint": "https://r-c50d6457.kinesisvideo.us-east-1.amazonaws.com"
      },
      {
        "Protocol": "WSS",
        "ResourceEndpoint": "wss://m-b94e17e0.kinesisvideo.us-east-1.amazonaws.com"
      }
    ]
  }
  ```

## 7. ICE Server Configuration
- Transitions to "Get ICE Server Configuration" state
- Sends API request to: `https://r-c50d6457.kinesisvideo.us-east-1.amazonaws.com/v1/get-ice-server-config`
- Request body:
  ```json
  {
    "ChannelARN": "arn:aws:kinesisvideo:us-east-1:995678934610:channel/demo-channel/1712136149494",
    "ClientId": "ProducerMaster",
    "Service": "TURN"
  }
  ```
- API Response:
  ```json
  {
    "IceServerList": [
      {
        "Password": "nhgUYYY0TKW5knkHAU92JI9whmOO058qbmxRvyIY2Rg=",
        "Ttl": 300,
        "Uris": [
          "turn:13-217-27-141.t-99577840.kinesisvideo.us-east-1.amazonaws.com:443?transport=udp",
          "turns:13-217-27-141.t-99577840.kinesisvideo.us-east-1.amazonaws.com:443?transport=udp",
          "turns:13-217-27-141.t-99577840.kinesisvideo.us-east-1.amazonaws.com:443?transport=tcp"
        ],
        "Username": "1743506463:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtZWFzdC0xOjk5NTY3ODkzNDYxMDpjaGFubmVsL2RlbW8tY2hhbm5lbC8xNzEyMTM2MTQ5NDk0"
      },
      {
        "Password": "O1ABsajTCFta+wj/9tTp8yF5aOFoiG6k/sWL3F+QT6w=",
        "Ttl": 300,
        "Uris": [
          "turn:54-159-217-207.t-99577840.kinesisvideo.us-east-1.amazonaws.com:443?transport=udp",
          "turns:54-159-217-207.t-99577840.kinesisvideo.us-east-1.amazonaws.com:443?transport=udp",
          "turns:54-159-217-207.t-99577840.kinesisvideo.us-east-1.amazonaws.com:443?transport=tcp"
        ],
        "Username": "1743506463:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtZWFzdC0xOjk5NTY3ODkzNDYxMDpjaGFubmVsL2RlbW8tY2hhbm5lbC8xNzEyMTM2MTQ5NDk0"
      }
    ]
  }
  ```

## 8. Signaling Connection
- Transitions to "Ready" state once all configurations are obtained
- Begins connection to the WSS endpoint: `wss://m-b94e17e0.kinesisvideo.us-east-1.amazonaws.com`
- Fully qualified URL example: `wss://m-07e4d66a.kinesisvideo.us-east-1.amazonaws.com?X-Amz-ChannelARN=arn:aws:kinesisvideo:us-east-1:995678934610:channel/demo-channel/1689337250341`
- Successfully establishes WebSocket connection
- Transitions to "Connected" state when the connection is established

## 9. WebRTC Peer Connection (After Connected State)
- Once connected, the signaling client can exchange SDP offers/answers and ICE candidates
- WebRTC peer connection is established using the acquired ICE server configurations
- Media streaming begins after successful connection
