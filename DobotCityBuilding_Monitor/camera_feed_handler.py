import time

import cv2

import base_handler


class CameraFeedHandler(base_handler.BaseHandler):

	CAPTURE_DELAY = 50  # ms, how often to capture an image

	# # # LINKS & INSTANCES # # #
	capture: cv2.VideoCapture

	# # # OUTPUT DATA # # #
	# Last capture to have been performed
	feed = None
	# Last capture successful
	success = False
	# Last capture saved
	saved = False

	def init(self):
		self.capture = cv2.VideoCapture(1, cv2.CAP_DSHOW)

	def update(self):
		while self.running:
			self.saved = False
			self.success, self.feed = self.capture.read()

			if not self.success:
				# If the capture failed, the camera might be disconnected
				self.feed = cv2.imread("resources/camera_noise.png")
				self.capture = cv2.VideoCapture(1, cv2.CAP_DSHOW)
			time.sleep(self.CAPTURE_DELAY / 1000)

	def stop_actions(self):
		pass
