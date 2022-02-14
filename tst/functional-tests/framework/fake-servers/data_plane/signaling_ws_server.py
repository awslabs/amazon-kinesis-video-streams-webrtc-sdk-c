#!/usr/bin/env python

# This script is a relay websockets server to exchange
# data plane messages (Offers, ICE candidates etc.) between WebRTC peers

import asyncio
import logging

from asyncio.log import logger
from websockets import serve
from websockets import WebSocketServerProtocol
from threading import Lock

# Flags to indicate wether master and viewer has connected to the websockets server
# Note that booleans are by default atomic in python
master_socket_available = False
viewer_socket_available = False

socket_map = {}

pending_messages_for_master = []
pending_messages_for_viewer = []

master_message_lock = Lock()
viewer_message_lock = Lock()

# Run periodic routine every 1 second and flush
# pending messages to master and viewer
#@asyncio.coroutine
async def periodic_message_dispatcher():
    while True:
        send_pending_messages_to_master()
        send_pending_messages_to_viewer()
        await asyncio.sleep(1)

# Send all pending messages received from viewer to master
def send_pending_messages_to_master():
    if not master_socket_available:
        return

    master_socket = socket_map['master_socket']

    master_message_lock.acquire()
    temp_pending_messages_for_master = pending_messages_for_master
    pending_messages_for_master.clear()
    master_message_lock.release()

    for msg in temp_pending_messages_for_master:
        master_socket.send(msg)

# Send all pending messages received from master to viewer
def send_pending_messages_to_viewer():
    if not viewer_socket_available:
        return

    viewer_socket = socket_map['viewer_socket']

    master_message_lock.acquire()
    temp_pending_messages_for_viewer = pending_messages_for_viewer
    pending_messages_for_viewer.clear()
    master_message_lock.release()

    for msg in temp_pending_messages_for_viewer:
        viewer_socket.send(msg)

async def handle_message_from_master(websocket, message):
    viewer_message_lock.acquire()
    pending_messages_for_viewer.append(message)
    viewer_message_lock.release()

async def handle_message_from_viewer(message):
    master_message_lock.acquire()
    pending_messages_for_master.append(message)
    master_message_lock.release()

def isViewerSocket(path):
    # Viewer connects with X-Amz-ClientId query param whereas master does not
    # Hence we use X-Amz-ClientId as an identifier to distinguish between
    # master and viewer socket
    return 'X-Amz-ClientId' in path

async def register_socket_if_not_already_registered(websocket, path):

    # Both the master and viewer has been connected
    if len(socket_map) == 2:
        return

    isViewerSock = 'X-Amz-ClientId' in path
    # This logic is problematic if there are multiple viewers and one master
    # But for webrtc tests, we would be running one master and one viewer
    if isViewerSock:
        socket_map['viewer_socket'] = websocket
        viewer_socket_available = True
    else:
        socket_map['master_socket'] = websocket
        master_socket_available = True

# handle messages sent by the viewer
async def viewer_message_handler(message):
    await handle_message_from_viewer(message)

# handle messages sent by the master
async def master_message_handler(message):
    await handle_message_from_master(message)

async def get_message_handler(path):
    if (isViewerSocket(path)):
        return viewer_message_handler
    else:
        return master_message_handler

async def messageHandler(websocket, path):
    await register_socket_if_not_already_registered(websocket, path)
    message_handler = await get_message_handler(path)

    isMessageFromViewer = isViewerSocket(path)
    async for message in websocket:
        if isMessageFromViewer:
            await viewer_message_handler(message)
        else:
            await master_message_handler(message)

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
