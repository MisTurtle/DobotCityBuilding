from tkinter import *
from PIL import Image as PilImage, ImageTk

import ArucoCrop.CV2_ArucoCrop

import bluetooth_handler
import camera_utils
from base_handler import *
from camera_feed_handler import CameraFeedHandler


class MonitorHandler(FrameHoldingBaseHandler):

	# Button ids
	B_CALIBRATE = 0
	B_BUILD = 1
	B_RESET = 2
	B_CAMERA_CALIB = 3
	B_DRAW = 4
	B_QUIT = 5

	BTN_COUNT = 6

	active_btn = -1
	exit_to_calibration = False  # TODO : Change this to an integer representing the destination

	# # # tKinter Elements # # #
	btn_width = 0
	btnCmds = []
	btnImages = [[], []]
	btnInstances = []

	conn_err_frame: Frame
	conn_err_symbol: PhotoImage
	conn_err_text: Label

	info_text: Label
	info_embed = "[INFO]\n\n{}"

	# # # Handlers # # #
	bluetooth_h: bluetooth_handler.BluetoothHandler

	def __init__(self, hid: int):
		super().__init__(hid)

	def init(self):
		"""
		Init all the tkinter elements for this window
		"""
		super().init()

		# 1 second between each image processing,
		# Can be taken down if some kind of result filtering is added
		# and detected blocks are only transmitted once every x seconds
		# Otherwise it will result in an overflow of the communication
		self.REFRESH_DELAY = 1000

		# Retrieve a reference to the Bluetooth handler
		self.bluetooth_h = BaseHandler.handlers[HandlerId.BLUETOOTH]

		# Retrieve Metrics
		screen_width = self.window.winfo_screenwidth()
		screen_height = self.window.winfo_screenheight()

		self.btn_width = int(0.29 * screen_width)

		# Create Buttons
		self.init_btn()
		self.btnInstances.clear()
		for i in range(self.BTN_COUNT):
			img = self.btnImages[0][i]
			x = int((0.03 + (0.305 * (i % 3))) * screen_width)
			y = int(screen_height - img.height() * (1.25 + 1.1 * (1 - (i // 3))))
			button = Button(self.window, command=self.btnCmds[i], image=img, borderwidth=0, bg=self.window_bg, activebackground=self.window_bg)
			button.place(x=x, y=y, anchor=NW)
			self.btnInstances.append(button)

		self.info_text = Label(self.window, font=('Arial', 12))
		self.set_info("Aucune information à afficher actuellement", "#000000")
		self.info_text.place(relx=0.5, rely=0.6, anchor='n')

		# Create bluetooth error frame
		self.conn_err_frame = Frame(self.window, width=0.8*screen_width, height=0.6*screen_height, bg="#d5c9a0")

		img = PilImage.open("resources/conn.png")
		img = img.resize((150, 150), PilImage.ANTIALIAS)
		self.conn_err_symbol = ImageTk.PhotoImage(img)

		self.conn_err_text = Label(self.conn_err_frame, image=self.conn_err_symbol,
			text="   La connexion au module bluetooth a été perdue !\n   Vérifiez que...\n\n"
				"   1- Le PC est bien connecté au réseau bluetooth\n"
			    "   2- Les identifiants de connexion en Bluetooth sont corrects\n"
				"   3- Les Dobots sont connectés au boitier de controle\n",
			compound=LEFT, justify=LEFT, font=('Arial', 14), padx = 20, bg="#d5c9a0")
		self.conn_err_text.place(relx=0.5, rely=0.5, anchor=CENTER)

	def init_btn(self):
		"""
		Init Button Data (Image and text)
		"""
		self.btnImages = [
			[
				PilImage.open("resources/buttons/BTN_CalibRequest.png"),
				PilImage.open("resources/buttons/BTN_BuildStart.png"),
				PilImage.open("resources/buttons/BTN_Restart.png"),
				PilImage.open("resources/buttons/BTN_CalibCamera.png"),
				PilImage.open("resources/buttons/BTN_Draw.png"),
				PilImage.open("resources/buttons/BTN_Quit.png")
			],
			[
				PilImage.open("resources/buttons/BTN_CalibConfirm.png"),
				PilImage.open("resources/buttons/BTN_BuildStop.png"),
				None,
				None,
				None,
				None
			]
		]
		self.btnCmds = [
			self.Handle_Calibration,
			self.Handle_Build,
			self.Handle_Reset,
			self.Handle_CameraCalib,
			self.Handle_Draw,
			self.Handle_Quit
		]
		for i in range(len(self.btnImages)):
			for j in range(len(self.btnImages[i])):
				img = self.btnImages[i][j]
				if img is None:
					continue
				self.btnImages[i][j] = ImageTk.PhotoImage(img.resize((self.btn_width, int(self.btn_width * img.height/img.width))))

	def update(self):
		if self.running:
			# Camera Feed
			success, feed = self.get_camera_feed()
			if feed is not None:
				if success:
					# Capture successful, process the last frame with ArucoCrop and camera_utils.process_storage
					ArucoCrop.CV2_ArucoCrop.process_frame(feed)
					# Send results over bluetooth
					BaseHandler.handlers[HandlerId.BLUETOOTH].send_blocks(camera_utils.compounds)
					result = camera_utils.last_result
					if result is None:
						result = feed
					if not camera_utils.last_success:
						self.set_info("Erreur lors de la détection de la zone de stockage...", "#aa0000")
					else:
						self.set_info("Détection de {} cubes lors de la dernière capture...".format(camera_utils.last_count), "#00aa00")
					camera_utils.last_success = False
				else:
					# Clear the Arduino memory of the last detected compounds
					BaseHandler.handlers[HandlerId.BLUETOOTH].send_blocks([])
					result = feed

				self.save_result(result)

			# Display Bluetooth Status
			if self.bluetooth_h.online:
				self.conn_err_frame.place_forget()
				self.enable_buttons()
			else:
				self.conn_err_frame.place(relx=0.5, rely=0.5, anchor=CENTER)
				self.disable_buttons()

		self.window.after(self.REFRESH_DELAY, self.update)

	def enable_buttons(self):
		for bid, btn in enumerate(self.btnInstances):
			if bid != self.B_QUIT:
				btn.config(state='normal' if self.active_btn == -1 or self.active_btn == bid else 'disabled')

	def disable_buttons(self):
		for bid, btn in enumerate(self.btnInstances):
			if bid != self.B_QUIT:
				btn.config(state='disabled')

	# # # HANDLING FUNCTIONS # # #
	def Handle_ClickOn(self, btn_id: int) -> bool:
		if self.active_btn != -1 and self.active_btn != btn_id:
			return False
		active = self.active_btn != btn_id

		img = self.btnImages[int(active)][btn_id]
		if img is not None:
			self.btnInstances[btn_id].config(image=img)

		self.active_btn = btn_id if active else -1
		for bid, btn in enumerate(self.btnInstances):
			if bid != btn_id and bid != self.B_QUIT:
				btn.config(state=('disabled' if active else 'normal'))
		return True

	def Handle_Calibration(self):
		if self.Handle_ClickOn(self.B_CALIBRATE):
			if self.active_btn == self.B_CALIBRATE:
				# Send calibration request
				self.bluetooth_h.send_calib_request()
			else:
				# Send calibration confirmation
				self.bluetooth_h.send_calib_confirmation()

	def Handle_Build(self):
		if self.Handle_ClickOn(self.B_BUILD):
			if self.active_btn == self.B_BUILD:
				self.bluetooth_h.send_mode(1)  # Start building
			else:
				self.bluetooth_h.send_mode(0)  # Go Idle

	def Handle_Draw(self):
		if self.Handle_ClickOn(self.B_DRAW):
			if self.active_btn == self.B_DRAW:
				self.bluetooth_h.send_mode(2)  # TODO : Drawing interface
			else:
				self.bluetooth_h.send_mode(0)  # Go Idle

	def Handle_Reset(self):
		self.bluetooth_h.send_reset()

	def Handle_CameraCalib(self):
		self.exit_to_calibration = True
		self.Handle_Quit()

	def Handle_Quit(self):
		self.stop()

	def set_info(self, text: str, fg: str):
		self.info_text.config(text=self.info_embed.format(text), fg=fg)

	def stop_actions(self):
		super().stop_actions()
		self.bluetooth_h.send_mode(0)
