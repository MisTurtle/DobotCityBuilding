from tkinter import Tk

import bluetooth_handler
import camera_utils
import monitor_handler
from base_handler import HandlerId

import camera_calibration_handler
import camera_feed_handler


MonitorHandler: monitor_handler.MonitorHandler
CalibrationHandler: camera_calibration_handler.CameraCalibrationHandler


def StartCameraCalibration():
	global CalibrationHandler
	CalibrationHandler = camera_calibration_handler.CameraCalibrationHandler(HandlerId.CALIBRATION)
	CalibrationHandler.start()
	CalibrationHandler.window.mainloop()


def StartMonitor():
	global MonitorHandler
	MonitorHandler = monitor_handler.MonitorHandler(HandlerId.MONITOR)
	MonitorHandler.start()
	MonitorHandler.window.mainloop()


# Init ArucoCrop library
camera_utils.init_aruco_crop()

# Start a Camera Feed
CameraFeed = camera_feed_handler.CameraFeedHandler(HandlerId.CAMERA_FEED)
CameraFeed.start()

# Try to establish a bluetooth connection
BluetoothHandler = bluetooth_handler.BluetoothHandler(HandlerId.BLUETOOTH)
BluetoothHandler.start()

# Launch the camera calibration window
StartCameraCalibration()

# Main loop to jump between windows
while True:
	StartMonitor()
	if MonitorHandler.exit_to_calibration:
		MonitorHandler.exit_to_calibration = False
		StartCameraCalibration()
	else:
		break

if BluetoothHandler.online:
	# Go Idle
	# It doesn't matter if the bluetooth module can't be reached
	# because after 5 seconds of inactivity, the arduino will stop on its own
	BluetoothHandler.send_mode(0)
# Stop other threads
BluetoothHandler.send_mode(0)
BluetoothHandler.stop()
CameraFeed.stop()
