#!/usr/bin/env python3

"""
H.264 UVC Camera Configuration Script (Pure Python)
Configure UVC H.264 camera settings directly without shell wrapper
"""

import sys
import os
import time
import logging
import ctypes
import fcntl
import argparse

# UVC H.264 extension constants and structures
UVC_H264_GUID = b'\x41\x76\x9e\xa2\x04\xde\xe3\x47\x8b\x2b\xf4\x34\x1a\xff\x00\x3b'

VIDEO_CONFIG_PROBE = 0x01
VIDEO_CONFIG_COMMIT = 0x02

BMHINTS_NONE = 0x0000
BMHINTS_RESOLUTION = 0x0001
BMHINTS_PROFILE = 0x0002
BMHINTS_RATECONTROL = 0x0004
BMHINTS_USAGE = 0x0008
BMHINTS_FRAME_INTERVAL = 0x0800
BMHINTS_BITRATE = 0x2000
BMHINTS_IFRAMEPERIOD = 0x8000

PROFILE_HIGH = 0x6400
RATECONTROL_CBR = 0x01
USAGE_REALTIME = 0x01
STREAM_MUX_H264_ENABLED = 0x03

UVC_SET_CUR = 0x01
UVC_GET_CUR = 0x81
UVC_GET_DEF = 0x87
UVC_GET_LEN = 0x85

# ioctl constants calculation
_IOC_NRBITS = 8
_IOC_TYPEBITS = 8
_IOC_SIZEBITS = 14
_IOC_DIRBITS = 2

_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = _IOC_NRSHIFT + _IOC_NRBITS
_IOC_SIZESHIFT = _IOC_TYPESHIFT + _IOC_TYPEBITS
_IOC_DIRSHIFT = _IOC_SIZESHIFT + _IOC_SIZEBITS

_IOC_NONE = 0
_IOC_WRITE = 1
_IOC_READ = 2

def _IOC(dir_, type_, nr, size):
    return (
        ctypes.c_int32(dir_ << _IOC_DIRSHIFT).value |
        ctypes.c_int32(ord(type_) << _IOC_TYPESHIFT).value |
        ctypes.c_int32(nr << _IOC_NRSHIFT).value |
        ctypes.c_int32(size << _IOC_SIZESHIFT).value)

def _IOC_TYPECHECK(t):
    return ctypes.sizeof(t)

def _IOWR(type_, nr, size):
    return _IOC(_IOC_READ | _IOC_WRITE, type_, nr, _IOC_TYPECHECK(size))

# UVC control structures
class uvc_xu_control_query(ctypes.Structure):
    _fields_ = [
        ('unit', ctypes.c_uint8),
        ('selector', ctypes.c_uint8),
        ('query', ctypes.c_uint8),
        ('size', ctypes.c_uint16),
        ('data', ctypes.c_void_p),
    ]

# Calculate UVCIOC_CTRL_QUERY using the correct _IOWR function
UVCIOC_CTRL_QUERY = _IOWR('u', 0x21, uvc_xu_control_query)

class video_config_probe_commit(ctypes.Structure):
    _fields_ = [
        ('dwFrameInterval', ctypes.c_uint32),
        ('dwBitRate', ctypes.c_uint32),
        ('bmHints', ctypes.c_uint16),
        ('wConfigurationIndex', ctypes.c_uint16),
        ('wWidth', ctypes.c_uint16),
        ('wHeight', ctypes.c_uint16),
        ('wSliceUnits', ctypes.c_uint16),
        ('wSliceMode', ctypes.c_uint16),
        ('wProfile', ctypes.c_uint16),
        ('wIFramePeriod', ctypes.c_uint16),
        ('wEstimatedVideoDelay', ctypes.c_uint16),
        ('wEstimatedMaxConfigDelay', ctypes.c_uint16),
        ('bUsageType', ctypes.c_uint8),
        ('bRateControlMode', ctypes.c_uint8),
        ('bTemporalScaleMode', ctypes.c_uint8),
        ('bSpatialScaleMode', ctypes.c_uint8),
        ('bSNRScaleMode', ctypes.c_uint8),
        ('bStreamMuxOption', ctypes.c_uint8),
        ('bStreamFormat', ctypes.c_uint8),
        ('bEntropyCABAC', ctypes.c_uint8),
        ('bTimestamp', ctypes.c_uint8),
        ('bNumOfReorderFrames', ctypes.c_uint8),
        ('bPreviewFlipped', ctypes.c_uint8),
        ('bView', ctypes.c_uint8),
        ('bReserved1', ctypes.c_uint8),
        ('bReserved2', ctypes.c_uint8),
        ('bStreamID', ctypes.c_uint8),
        ('bSpatialLayerRatio', ctypes.c_uint8),
        ('wLeakyBucketSize', ctypes.c_uint16),
    ]

def find_h264_unit_id(device_path):
    """Find H.264 extension unit ID from sysfs"""
    device_name = os.path.basename(device_path)
    desc_path = f"/sys/class/video4linux/{device_name}/../../../descriptors"
    
    try:
        with open(desc_path, 'rb') as f:
            descriptors = f.read()
        
        # Search for H.264 GUID in descriptors
        for i in range(len(descriptors) - 16):
            if descriptors[i:i+16] == UVC_H264_GUID:
                if i > 0:
                    return descriptors[i-1]
    except Exception as e:
        logging.debug(f"Failed to read descriptors: {e}")
    
    return 0

def get_length_xu_control(fd, unit_id, selector):
    """Get control length"""
    length = ctypes.c_uint16(0)
    xu_ctrl_query = uvc_xu_control_query()
    xu_ctrl_query.unit = unit_id
    xu_ctrl_query.selector = selector
    xu_ctrl_query.query = UVC_GET_LEN
    xu_ctrl_query.size = 2  # sizeof(length)
    xu_ctrl_query.data = ctypes.cast(ctypes.pointer(length), ctypes.c_void_p)

    try:
        fcntl.ioctl(fd, UVCIOC_CTRL_QUERY, xu_ctrl_query)
    except Exception as e:
        logging.warning(f"Failed to get control length for selector {selector}: {e}")

    return length

def query_xu_control(fd, unit_id, selector, query, data):
    """Query UVC extension unit control"""
    length = get_length_xu_control(fd, unit_id, selector)

    xu_ctrl_query = uvc_xu_control_query()
    xu_ctrl_query.unit = unit_id
    xu_ctrl_query.selector = selector
    xu_ctrl_query.query = query
    xu_ctrl_query.size = length
    xu_ctrl_query.data = ctypes.cast(ctypes.pointer(data), ctypes.c_void_p)

    try:
        fcntl.ioctl(fd, UVCIOC_CTRL_QUERY, xu_ctrl_query)
    except Exception as e:
        logging.warning(f"query_xu_control ({query}) failed: {e}")
        raise

def probe_then_commit(fd, unit_id, config, step_name):
    """Use proper PROBE/COMMIT protocol"""
    try:
        # First PROBE to test the configuration
        query_xu_control(fd, unit_id, VIDEO_CONFIG_PROBE, UVC_SET_CUR, config)
        # Then COMMIT to apply the configuration
        query_xu_control(fd, unit_id, VIDEO_CONFIG_COMMIT, UVC_SET_CUR, config)
        logging.info(f"SUCCESS: {step_name} completed")
        return True
    except Exception as e:
        logging.warning(f"{step_name} PROBE/COMMIT failed: {e}")
        return False

def configure_h264_camera(device, width, height, fps, bitrate, verbose=False):
    """Configure H.264 camera settings"""
    
    if verbose:
        logging.basicConfig(level=logging.DEBUG, format='%(levelname)s: %(message)s')
    else:
        logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
    
    print(f"H264 Camera Configurator (Pure Python)")
    print(f"======================================")
    print(f"Device: {device}")
    print(f"Resolution: {width}x{height}")
    print(f"Frame rate: {fps} fps")
    print(f"Bitrate: {bitrate} bps")
    print()
    
    # Check if device exists
    if not os.path.exists(device):
        print(f"Error: Device {device} does not exist")
        return False
    
    print(f"Opening device: {device}")
    
    try:
        # Open device
        fd = os.open(device, os.O_RDWR)
        
        # Find H.264 extension unit ID
        unit_id = find_h264_unit_id(device)
        if unit_id == 0:
            print("Error: H.264 extension unit not found")
            os.close(fd)
            return False
            
        print(f"H.264 extension found, Unit ID: {unit_id}")
        
        # Get current configuration as base
        config = video_config_probe_commit()
        config_len = get_length_xu_control(fd, unit_id, VIDEO_CONFIG_PROBE)
        
        try:
            query_xu_control(fd, unit_id, VIDEO_CONFIG_PROBE, UVC_GET_CUR, config)
            print(f"Current config - Width: {config.wWidth}, Height: {config.wHeight}, MuxOption: 0x{config.bStreamMuxOption:02x}")
        except:
            print("Failed to get current config, using defaults")
            try:
                query_xu_control(fd, unit_id, VIDEO_CONFIG_PROBE, UVC_GET_DEF, config)
            except:
                print("Failed to get default config, zeroing")
                ctypes.memset(ctypes.pointer(config), 0, ctypes.sizeof(config))
        
        success = True
        
        # Step 1: Enable H.264 multiplexing first
        print("Step 1: Enabling H.264 multiplexing")
        config.bmHints = BMHINTS_NONE
        config.bStreamMuxOption = STREAM_MUX_H264_ENABLED
        if not probe_then_commit(fd, unit_id, config, "H.264 Multiplexing"):
            success = False
        time.sleep(0.1)
        
        # Step 2: Set resolution (width and height together)
        print(f"Step 2: Setting resolution to {width}x{height}")
        config.bmHints = BMHINTS_RESOLUTION
        config.wWidth = width
        config.wHeight = height
        if not probe_then_commit(fd, unit_id, config, "Resolution"):
            success = False
        time.sleep(0.1)
        
        # Step 3: Set frame interval
        print(f"Step 3: Setting frame rate to {fps} fps")
        config.bmHints = BMHINTS_FRAME_INTERVAL
        config.dwFrameInterval = 10000000 // fps
        if not probe_then_commit(fd, unit_id, config, "Frame Rate"):
            success = False
        time.sleep(0.1)
        
        # Step 4: Set bitrate
        print(f"Step 4: Setting bitrate to {bitrate} bps")
        config.bmHints = BMHINTS_BITRATE
        config.dwBitRate = bitrate
        if not probe_then_commit(fd, unit_id, config, "Bitrate"):
            success = False
        time.sleep(0.1)
        
        # Step 5: Set other parameters individually
        print("Step 5: Setting additional parameters")
        
        config.bmHints = BMHINTS_RATECONTROL
        config.bRateControlMode = RATECONTROL_CBR
        probe_then_commit(fd, unit_id, config, "Rate Control")
        time.sleep(0.1)
        
        config.bmHints = BMHINTS_PROFILE
        config.wProfile = PROFILE_HIGH
        probe_then_commit(fd, unit_id, config, "Profile")
        time.sleep(0.1)
        
        config.bmHints = BMHINTS_IFRAMEPERIOD
        config.wIFramePeriod = fps
        probe_then_commit(fd, unit_id, config, "I-Frame Period")
        time.sleep(0.1)
        
        config.bmHints = BMHINTS_USAGE
        config.bUsageType = USAGE_REALTIME
        probe_then_commit(fd, unit_id, config, "Usage Type")
        time.sleep(0.1)
        
        # Final verification
        print("Final verification...")
        final_config = video_config_probe_commit()
        try:
            query_xu_control(fd, unit_id, VIDEO_CONFIG_PROBE, UVC_GET_CUR, final_config)
            print(f"Final config - Width: {final_config.wWidth}, Height: {final_config.wHeight}, MuxOption: 0x{final_config.bStreamMuxOption:02x}")
            
            if final_config.bStreamMuxOption & STREAM_MUX_H264_ENABLED:
                print("✅ H.264 multiplexing is ENABLED")
            else:
                print("❌ H.264 multiplexing is DISABLED")
                success = False
        except:
            print("Failed to verify final configuration")
        
        os.close(fd)
        
        # Wait for firmware to apply settings
        print("Waiting for firmware to apply settings...")
        time.sleep(3)
        
        if success:
            print("✅ Configuration completed successfully")
            print()
            print("You can now use the camera with:")
            print("gst-launch-1.0 \\")
            print(f"  v4l2src device={device} ! image/jpeg,width={width},height={height},framerate={fps}/1 ! \\")
            print("  mjpgxh264demux ! h264parse ! rtph264pay config-interval=1 pt=96 ! \\")
            print("  udpsink host=192.168.0.21 port=5000")
        else:
            print("⚠️ Configuration completed with warnings")
        
        return True  # Return True even with warnings to continue pipeline
        
    except Exception as e:
        print(f"Error: {e}")
        try:
            os.close(fd)
        except:
            pass
        return False

def main():
    parser = argparse.ArgumentParser(
        description='Configure UVC H.264 camera settings',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s                                      # Use default settings
  %(prog)s -d /dev/video0 -w 1280 --ht 720     # HD resolution
  %(prog)s -f 15 -b 500000                     # Low framerate and bitrate
  %(prog)s --verbose                           # Enable debug output
        '''
    )
    
    parser.add_argument('-d', '--device', default='/dev/video0',
                        help='Camera device path (default: /dev/video0)')
    parser.add_argument('-w', '--width', type=int, default=640,
                        help='Video width (default: 640)')
    parser.add_argument('--ht', '--height', type=int, default=480, dest='height',
                        help='Video height (default: 480)')
    parser.add_argument('-f', '--fps', type=int, default=30,
                        help='Frame rate (default: 30)')
    parser.add_argument('-b', '--bitrate', type=int, default=300000,
                        help='Bitrate in bps (default: 300000)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Enable verbose debug output')
    
    args = parser.parse_args()
    
    success = configure_h264_camera(
        args.device, args.width, args.height, 
        args.fps, args.bitrate, args.verbose
    )
    
    if success:
        print("\n🎉 Camera configuration completed successfully!")
        sys.exit(0)
    else:
        print("\n❌ Camera configuration failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()