#!/usr/bin/env python

# This script is a relay websockets server to exchange
# data plane messages (Offers, ICE candidates etc.) between WebRTC peers

import asyncio
import logging

from asyncio.log import logger
from websockets import serve
from websockets import WebSocketServerProtocol
from threading import Lock
import json

socket_map = {}

pending_messages_for_master = []
pending_messages_for_viewer = []

master_message_lock = Lock()
viewer_message_lock = Lock()

globalCorrelationId = 10

###########################################################################

class StatusResponse:
    def __init__(self):
        self.correlationId = 1

class PeerRequest:
    def __init__(self, payload):
        self.__dict__ = json.loads(payload)

class PeerEvent:
    def __init__(self, senderClientId, action, messagePayload):
        self.senderClientId = senderClientId
        self.messageType = action
        self.messagePayload = messagePayload
        self.statusResponse = 1

async def convert_to_async_message_format(msg):
    peer_request = PeerRequest(msg)
    peer_event = PeerEvent(peer_request.RecipientClientId, peer_request.action, peer_request.MessagePayload)    
    return json.dumps(peer_event.__dict__)


###########################################################################


# Run periodic routine every 1 second and flush
# pending messages to master and viewer
#@asyncio.coroutine
async def periodic_message_dispatcher():
    while True:
        await send_pending_messages_to_master()
        await send_pending_messages_to_viewer()
        await asyncio.sleep(2)

# Send all pending messages received from viewer to master
async def send_pending_messages_to_master():
    if "master" not in socket_map:
        return

    master_message_lock.acquire()
    for msg in pending_messages_for_master:
        print("\n Sending to Master - ")
        print(msg)
        await socket_map['master'].send(msg)
    pending_messages_for_master.clear() 
    master_message_lock.release()

# Send all pending messages received from master to viewer
async def send_pending_messages_to_viewer():
    if "viewer" not in socket_map:
        return

    viewer_message_lock.acquire()
    for msg in pending_messages_for_viewer:
        print("\n Sending to Viewer - ")
        print(msg)
        await socket_map['viewer'].send(msg)
    pending_messages_for_viewer.clear() 
    viewer_message_lock.release()

async def handle_message_from_master(message):
    msg = await convert_to_async_message_format(message)
    print("\n ** Master sent -")
    print(message)
    viewer_message_lock.acquire()
    pending_messages_for_viewer.append(msg)
    viewer_message_lock.release()

async def handle_message_from_viewer(message):
    msg = await convert_to_async_message_format(message)
    print("\n ** Viewer sent -")
    print(message)
    master_message_lock.acquire()
    pending_messages_for_master.append(msg)
    master_message_lock.release()

def isViewerSocket(path):
    # Viewer connects with X-Amz-ClientId query param whereas master does not
    # Hence we use X-Amz-ClientId as an identifier to distinguish between
    # master and viewer socket
    return 'X-Amz-ClientId' in path

async def messageHandler(websocket, path):
    # await register_socket_if_not_already_registered(websocket, path)
    # message_handler = await get_message_handler(path)

    isViewer = isViewerSocket(path)
    if isViewer:
        print("Viewer connected")
        socket_map['viewer'] = websocket
    else:
        print("Master connected")
        socket_map['master'] = websocket

    async for message in websocket:
        if isViewer:
            await handle_message_from_viewer(message)
        else:
            await handle_message_from_master(message)

async def main():
    # Setup a message dispatcher to flush pending messages to master and viewer
    asyncio.Task(periodic_message_dispatcher())
    # start the server
    async with serve(messageHandler, "localhost", 8765):
        await asyncio.Future()  # run forever

def setup_logger():
    #TODO: configure logger for the server
    logging.basicConfig(filename="/tmp/websockets-log",
                        filemode='a',
                        format='%(asctime)s,%(msecs)d %(name)s %(levelname)s %(message)s',
                        datefmt='%H:%M:%S',
                        level=logging.DEBUG)

#if __name__ == "__main__":
setup_logger()
asyncio.run(main())
