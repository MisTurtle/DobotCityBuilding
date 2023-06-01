import camera_utils
from base_handler import *
from PIL import Image as PILImage, ImageTk


class CameraCalibrationHandler(FrameHoldingBaseHandler):
	"""
	Interface to visualize the effect of given parameters on the camera feed
	"""

	# # # tKinter Elements # # #
	confirm_button: Button
	previous_button: Button
	btnImages = []

	shadow_size: Scale
	shadow_intensity: Scale
	info_text: Label
	info_embed = "[INFO]\n\n{}"  # User information on what to do

	# # # Window State # # #
	# 0 is searching for Aruco Markers
	# 1 is calibrating Canny parameters
	state: int = 0

	def init(self):
		super().init()
		sw, sh = self.window.winfo_screenwidth(), self.window.winfo_screenheight()

		btn_width = int(sw * 0.29)
		self.btnImages = [
			PILImage.open("resources/buttons/BTN_CameraCalibNext.png"),
			PILImage.open("resources/buttons/BTN_CameraCalibPrevious.png"),
			PILImage.open("resources/buttons/BTN_CameraCalibConfirm.png")
		]
		for i in range(len(self.btnImages)):
			img = self.btnImages[i]
			self.btnImages[i] = ImageTk.PhotoImage(img.resize((btn_width, int(btn_width * img.height / img.width))))

		self.confirm_button = Button(self.window, command=self.next_state, image=self.btnImages[0], bg=self.window_bg, activebackground=self.window_bg, borderwidth=0)
		self.confirm_button.place(relx=0.65, y=sh - int(1.2 * self.btnImages[0].height()), anchor='n')

		self.previous_button = Button(self.window, command=self.previous_state, image=self.btnImages[1], bg=self.window_bg, activebackground=self.window_bg, borderwidth=0, state="disabled")
		self.previous_button.place(relx=0.35, y=sh - int(1.2 * self.btnImages[0].height()), anchor='n')

		def update_shadow_size(val):
			camera_utils.sl_shadow_size = int(val)

		def update_shadow_intensity(val):
			camera_utils.sl_shadow_intensity = int(val)

		self.shadow_size = Scale(self.window, from_=0, to=1000, resolution=2, length=int(0.7 * sw), orient=HORIZONTAL,
							command=update_shadow_size, label="Taille des ombres", bg=self.window_bg, fg="white")
		self.shadow_intensity = Scale(self.window, from_=0, to=1000, length=int(0.7 * sw), orient=HORIZONTAL,
							command=update_shadow_intensity, label="Intensité des ombres", bg=self.window_bg, fg="white")

		self.info_text = Label(self.window, font=('Arial', 12))
		self.set_info("Aucune information à afficher actuellement...", "#000000")
		self.info_text.place(relx=0.5, rely=0.45, anchor='n')

	def next_state(self, previous: bool = False):
		"""
		Called when the 'Next' button (or 'Previous') is pressed
		Switch modes and show / hide calibration elements
		"""
		if previous:
			self.state -= 1
		else:
			self.state += 1

		match self.state:
			case 0:
				self.shadow_size.pack_forget()
				self.shadow_size.place_forget()
				self.shadow_intensity.pack_forget()
				self.shadow_intensity.place_forget()
				self.confirm_button.config(image=self.btnImages[0])
				self.previous_button.config(state="disabled")
			case 1:
				self.shadow_size.pack()
				self.shadow_size.place(relx=0.5, rely=0.58, anchor='n')
				self.shadow_size.set(camera_utils.sl_shadow_size)
				self.shadow_intensity.pack()
				self.shadow_intensity.place(relx=0.5, rely=0.65, anchor='n')
				self.shadow_intensity.set(camera_utils.sl_shadow_intensity)
				self.confirm_button.config(image=self.btnImages[2])
				self.previous_button.config(state="normal")
			case 2:
				camera_utils.sl_shadow_size = self.shadow_size.get()
				camera_utils.sl_shadow_intensity = self.shadow_intensity.get()
				self.confirm_button.config(state="disabled")
				self.previous_button.config(state="disabled")
				self.stop()

	def previous_state(self):
		"""
		Called when the 'Previous' button is pressed
		Switch modes and show / hide calibration elements
		"""
		self.next_state(True)

	def update(self):
		if self.running:
			success, feed = FrameHoldingBaseHandler.get_camera_feed()
			if feed is not None:
				if success:
					match self.state:
						case 0:
							result = camera_utils.detect_markers(feed)
							self.set_info("Placez la caméra de sorte à ce que les 4 marqueurs aux coins"
							              " de la zone de stockage soient détectés", "#00aa00")
						case 1:
							result = camera_utils.calib_show_contours(feed, camera_utils.sl_shadow_size, camera_utils.sl_shadow_intensity)
							self.set_info("Réglez les paramètres si dessous de sorte à réduire le bruit au maximum"
							              "\n tout en s'assurant que les blocs placés dans la zone soient entourés en vert", "#00aa00")
						case _:
							result = feed
					self.confirm_button.config(state='normal')
				else:
					result = feed
					self.set_info("Impossible de capturer une image... Vérifiez la connexion par USB de la caméra", "#aa0000")
					self.confirm_button.config(state='disabled')

				self.save_result(result)
			self.window.after(self.REFRESH_DELAY, self.update)

	def set_info(self, text: str, fg: str):
		self.info_text.config(text=self.info_embed.format(text), fg=fg)
