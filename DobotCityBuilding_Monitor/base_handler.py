import enum
import os.path
from tkinter import *
from abc import ABC, abstractmethod
from threading import Thread

import cv2


class HandlerId(enum.IntEnum):
	CAMERA_FEED = 0
	CALIBRATION = 1
	BLUETOOTH = 2
	MONITOR = 3


class BaseHandler(ABC):
	"""
	Base class for any handler in this program
	It contains basic methods such as init, update and stop
	"""

	# Static variable that contains the instances of each type of handler
	handlers: dict = {}

	# Class variable that determines whether the thread should keep running
	running = False
	# Thread currently running this handler's update method
	thread = None

	def __init__(self, hid: int):
		BaseHandler.handlers[hid] = self
		self.hid = hid
		self.init()

	@abstractmethod
	def init(self):
		"""
		Should initialize any field that will be needed later on
		Should be called right after being created
		"""
		pass

	@abstractmethod
	def update(self):
		"""
		Should be passed as the function to be executed in a thread
		Update fields initialized by the init function
		Should always check whether it needs to stop
		"""
		pass

	def start(self):
		self.running = True
		self.thread = Thread(target=self.update)
		self.thread.start()

	def stop(self):
		self.running = False
		self.thread.join(1)
		self.stop_actions()

	@abstractmethod
	def stop_actions(self):
		"""
		Code to be executed when the thread stops
		"""
		pass

	def get_hid(self) -> int:
		return self.hid

	def __del__(self):
		if self.running:
			self.running = False
			self.stop_actions()


class TkinterBaseHandler(BaseHandler, ABC):
	"""
	Extension of the BaseHandler class so that it adapts to the way
	tkinter windows work
	"""
	window: Tk

	def start(self):
		self.running = True
		self.update()

	def stop(self):
		self.running = False
		self.stop_actions()

	def stop_actions(self):
		self.window.destroy()


class FrameHoldingBaseHandler(TkinterBaseHandler, ABC):
	"""
	Extension to the TkinterBaseHandler class to take care of redondant stuff
	regarding the display of the camera feed
	"""

	@staticmethod
	def get_camera_feed():
		"""
		Fetch the most recent camera capture from the camera feed thread
		"""
		feed_handler = BaseHandler.handlers.get(HandlerId.CAMERA_FEED, None)
		if feed_handler is not None:
			return feed_handler.success, feed_handler.feed
		return False, None

	REFRESH_DELAY = 100  # ms, how often should we refresh the camera feed

	img_frame: Frame
	img_label: Label
	window: Tk

	window_bg = "#292929"

	def init(self):
		"""
		Instantiate the tkinter window as well as the frame shown on screen
		that contains the camera feed
		"""
		self.window = Tk()
		self.window.geometry("500x500")
		self.window.attributes('-fullscreen', True)
		self.window.configure(bg=self.window_bg)
		sw, sh = self.window.winfo_screenwidth(), self.window.winfo_screenheight()

		self.img_frame = Frame(self.window, width=0.9 * sw, height=0.9 * sh)
		self.img_frame.pack()
		self.img_frame.place(relx=0.5, y=10, anchor='n')

		self.img_label = Label(self.img_frame)
		self.img_label.pack()

	def save_result(self, result):
		"""
		Save the result as out/out.png file
		Also updates the camera feed shown on screen to display this new result instead
		"""
		cv2.imwrite("out/out.png", result)
		if os.path.exists("out/out.png"):
			self.img_label.photo = PhotoImage(file="out/out.png")
			self.img_label.config(image=self.img_label.photo)
