import enum
import math

from base_handler import *
from ArucoCrop import CV2_ArucoCrop as AC
from ArucoCrop.ArucoArea import CallbackArucoArea
import cv2
import numpy as np


class BlockType(enum.IntEnum):
	House = 0
	Building = 1
	Car = 2
	Tree = 3


# Dimensions of the storage area, in mm
storage_dimensions = 278, 104
# Coordinates in mm corresponding to the top left corner of the image
# once it has been cropped
camera_origin = 133, 319

# Shadow size read on the calibration slider
sl_shadow_size = 1
# Shadow intensity read on the calibration slider
sl_shadow_intensity = 1

# Color palette
palette = {
	BlockType.House: (0, 0, 255),
	BlockType.Building: (0, 200, 200),
	BlockType.Car: (255, 0, 0),
	BlockType.Tree: (0, 255, 0)
}
# Last image result
last_result = None
# Last success
last_success = False
# Last found compounds
compounds = []
# Last success cube count
last_count = 0


def init_aruco_crop():
	storage_zone = CallbackArucoArea(
		name="Storage Zone",
		aruco_id=10,
		marker_count=4,
		process_method=process_storage
	)
	AC.init(areas=[storage_zone], debug=True, debug_prefix="[ArucoCrop]")


def detect_markers(_frame: np.ndarray) -> np.ndarray:
	(_corners, _ids, _rejected) = cv2.aruco.detectMarkers(
		_frame,
		cv2.aruco.Dictionary_get(cv2.aruco.DICT_4X4_50),
		parameters=cv2.aruco.DetectorParameters_create()
	)

	for corners in _corners:
		corners = corners.astype(np.int0)
		cv2.circle(_frame, tuple(corners[0, 0]), 3, (255, 0, 0), 3)

	return cv2.aruco.drawDetectedMarkers(_frame, _corners, _ids, (0, 255, 0))


def canny_find_contours(_frame: np.ndarray, _block_size: int, _intensity: int):
	blurred = cv2.GaussianBlur(_frame, (5, 5), 0)
	canny = cv2.Canny(blurred, _block_size, _intensity)
	contours, _ = cv2.findContours(canny, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
	return contours


def calib_show_contours(_frame: np.ndarray, _block_size: int, _intensity: int) -> np.ndarray:
	contours = canny_find_contours(_frame, _block_size, _intensity)
	for cnt in contours:
		approx = cv2.approxPolyDP(cnt, 0.01 * cv2.arcLength(cnt, True), True)
		if 4 <= len(approx) <= 9:
			cv2.drawContours(_frame, [cnt], -1, (0, 255, 0), 3)
	return _frame
	# print(_block_size, _intensity)
	# kernel = np.ones((5, 5), np.uint8)
	# # Convert to grayscale
	# grayScale = cv2.cvtColor(_frame, cv2.COLOR_BGR2GRAY)
	# # Get a mask for the shadows
	# binaryMask = cv2.adaptiveThreshold(grayScale, 255, cv2.ADAPTIVE_THRESH_MEAN_C,
#                           cv2.THRESH_BINARY_INV, _block_size, _intensity)
	# # Blur to reduce noise
	# blurredMask = cv2.morphologyEx(binaryMask, cv2.MORPH_OPEN, kernel)
	# # Apply mask to remove shadows
	# return cv2.bitwise_and(_frame, _frame, mask=blurredMask)


def process_storage(self: CallbackArucoArea, image, rel_corners):
	global compounds, last_result, last_count, last_success

	width, height, rot, warped = self.rotate_and_crop(image, rel_corners)

	if width <= 0 or height <= 0:
		print("An error occured")
		return None

	# Detected Compounds output list, Registered center points
	compounds, centers_px = [], []
	# Compute horizontal and vertical ratio
	ratio = storage_dimensions[0] / width, storage_dimensions[1] / height
	# Detect contours in the cropped image
	contours = canny_find_contours(warped, sl_shadow_size, sl_shadow_intensity)
	# Validate each contour
	for cnt in contours:
		approx = cv2.approxPolyDP(cnt, 0.01 * cv2.arcLength(cnt, True), True)
		if 3 <= len(approx) <= 12:
			box = cv2.minAreaRect(approx)
			((cX, cY), (boxW, boxH), rot) = box
			boxSurfaceMM = (boxW * ratio[0]) * (boxH * ratio[1])
			if 20 * 20 * 0.7 <= boxSurfaceMM <= 20 * 20 * 1.7 and 0.65 <= boxW / boxH <= 1.35:
				dist = math.sqrt(cX ** 2 + cY ** 2)
				for center in centers_px:
					if abs(dist - center) < 10 * max(ratio[0], ratio[1]):
						break
				else:
					# Get the box's mean color by creating a mask around it
					rect = cv2.boxPoints(box).astype(np.int0)
					mask = np.zeros_like(warped)
					cv2.fillPoly(mask, [rect], (255, 255, 255))
					mask = cv2.cvtColor(mask, cv2.COLOR_BGR2GRAY)

					if cv2.countNonZero(mask) == 0:
						continue  # Mask would be all black for some reason

					mean_color = cv2.mean(warped, mask=mask)
					boxType = type_from_color(mean_color)

					# Register compound
					dx = width / 2 - cX
					hx = 29 / ratio[0]
					dy = cY - height / 2
					hy = 29 / ratio[1]

					correction = [0.5 * dx / hx, 3 * dy / hy]

					compounds.append(
						[
							camera_origin[0] + (cY + correction[1]) * ratio[0],
							camera_origin[1] - (cX + correction[0]) * ratio[1],
							rot,
							boxType
						]
					)
					centers_px.append(dist)

					cv2.circle(warped, (int(cX), int(cY)), 4, (255, 255, 255), -1)
					cX, cY = int(cX + correction[0]), int(cY + correction[1])
					# Draw valid contour
					cv2.drawContours(warped, [approx], -1, (0, 255, 0), 3)
					# Display its center point
					cv2.circle(warped, (int(cX), int(cY)), 4, (255, 0, 0), -1)
					# Display compound infos
					cv2.putText(warped, "Type : {}".format(boxType), (cX + 2, cY - 15), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 0), 1)
					cv2.putText(warped, "X : {}px".format(cX), (cX + 2, cY), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 0), 1)
					cv2.putText(warped, "Y : {}px".format(cY), (cX + 2, cY + 15), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 0), 1)
					cv2.putText(warped, "Rot : {:.2f}deg".format(rot), (cX + 2, cY + 30), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 0, 0), 1)

	last_count = len(compounds)
	last_result = warped
	last_success = True


def type_from_color(color) -> int:
	dist = None
	RGB_type = None
	for t, RGB in palette.items():
		length = ((RGB[0] - color[0])**2 + (RGB[1] - color[1])**2 + (RGB[2] - color[2])**2) ** 0.5
		if dist is None or length < dist:
			RGB_type = t
			dist = length
	if RGB_type == 0:
		print(color)
	return int(RGB_type)
