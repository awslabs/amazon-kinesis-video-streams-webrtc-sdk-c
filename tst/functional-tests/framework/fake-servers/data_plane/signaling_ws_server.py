#!/usr/bin/env python

# This script is a relay websockets server to exchange
# data plane messages (Offers, ICE candidates etc.) between WebRTC peers

import asyncio
import logging

from asyncio.log import logger
from websockets import serve
from websockets import WebSocketServerProtocol
from threading import Lock

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
        await send_pending_messages_to_master()
        await send_pending_messages_to_viewer()
        await asyncio.sleep(2)

# Send all pending messages received from viewer to master
async def send_pending_messages_to_master():
    if "master" not in socket_map:
        return

    master_message_lock.acquire()
    for msg in pending_messages_for_master:
        await socket_map['master'].send(msg)
    pending_messages_for_master.clear()
    master_message_lock.release()

# Send all pending messages received from master to viewer
async def send_pending_messages_to_viewer():
    if "viewer" not in socket_map:
        return

    viewer_message_lock.acquire()
    for msg in pending_messages_for_viewer:
        await socket_map['viewer'].send(msg)
    pending_messages_for_viewer.clear()
    viewer_message_lock.release()

async def handle_message_from_master(message):
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
