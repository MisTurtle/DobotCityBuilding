import time
from ctypes import Union

import serial
from serial.tools import list_ports

from base_handler import *


class BluetoothHandler(BaseHandler):
	"""
	Takes care of handling the bluetooth connection
	"""

	UPDATE_DELAY = 100  # ms, how often should we send newly added packets
	CONN_CHECK_INTERVAL = 3  # seconds, how often do we send a presence check packet

	# Mac Address of the HC-06 Module
	mac_addr = "98D351FE0B8C"
	# Local COM Port
	com_port = None
	# Bluetooth Serial Instance
	bluetooth_serial: serial.Serial = None
	# Packets to be sent
	outgoing_pks = []
	# Received buffer
	incoming_pk = ""
	# Current connection status
	online = False
	# Are we using the outgoing_pks list
	using = False
	# Last time we checked the bluetooth connection
	last_check = 0

	def init(self):
		pass

	def update(self):
		while self.running:
			# Try reconnecting if the connection broke
			if self.running and not self.online:
				self.reconnect()
			else:
				# Check if any packet needs to be sent
				if len(self.outgoing_pks) == 0 and time.time() - self.last_check > self.CONN_CHECK_INTERVAL:
					while self.using:
						pass
					# Send a presence packet to the Arduino
					print("[Bluetooth] Sending presence check")
					if "AAF15AA" not in self.outgoing_pks:
						self.outgoing_pks.append("AAF15AA")
					self.last_check = time.time()

				failures = 0
				while not self.using and len(self.outgoing_pks) > failures and self.online:
					# Loop through each packet to be sent
					try:
						# Write the buffer to the serial stream
						self.bluetooth_serial.write(bytes(self.outgoing_pks[failures], 'utf-8'))
						if not self.get_ack():
							# If the Arduino doesn't respond with a positive ack, skip this packet for now
							print("[Bluetooth] Failed to transmit packet {}".format(self.outgoing_pks[failures]))
							failures += 1
						else:
							# Otherwise, remove it from the packets to be sent and move on
							self.last_check = time.time()
							self.outgoing_pks.pop(failures)
					except serial.serialutil.SerialTimeoutException:
						print("[Bluetooth] TIMED OUT")
						self.online = False
						self.bluetooth_serial.close()
			time.sleep(self.UPDATE_DELAY / 1000)

	def reconnect(self) -> bool:
		"""
		Try to establish the bluetooth connection with the Arduino module
		"""
		print("[Bluetooth] Attempting connection with the bluetooth module")
		devices = list_ports.comports()
		self.com_port = None
		for device in devices:
			if self.mac_addr in device.hwid:
				self.com_port = device.usb_description()

		if self.com_port is None:
			print("[Bluetooth] Unable to identify the bluetooth module amongst available COM ports")
			return False
		else:
			try:
				self.bluetooth_serial = serial.Serial(self.com_port, 9600, writeTimeout=3, timeout=5)
				self.online = True
			except serial.serialutil.SerialException:
				print("[Bluetooth] Unable to identify the bluetooth module amongst available COM ports")
				time.sleep(1)
				return False
			print("[Bluetooth] Successfully connected to the module on {}".format(self.com_port))
		return True

	def stop_actions(self):
		pass

	def send_blocks(self, compounds):
		"""
		Clear the Arduino memory from all the register compounds,
		and send all those that have been detected in the previous frame
		"""
		self.outgoing_pks.append("AA00AA")  # Flush blocks
		for data in compounds:
			checksum = 1  # 1 for the packet id
			xPos, yPos, rot, t = hex(round(data[0] * 100))[2:].zfill(4), hex(round(data[1] * 100))[2:].zfill(4), hex(
				round((data[2] % 90) * 100))[2:].zfill(4), hex(data[3])[2]
			if len(xPos) != 4 or len(yPos) != 4 or len(rot) != 4:
				print("[Bluetooth] Unable to send block detected at x={} ; y={} ; rot={} of type {}".format(*data))
				continue
			for param in [xPos, yPos, rot, t]:
				for letter in param:
					for n in range(8):
						checksum += (int(letter, base=16) >> n) & 0x01
			checksum %= 10
			buffer = "AA1{}{}{}{}{}AA".format(
				xPos, yPos, rot, t, checksum
			)  # Create Compound Packet
			self.outgoing_pks.append(buffer)

	def send_calib_request(self):
		"""
		Called when the Calibration button is pressed for the first time
		"""
		self.outgoing_pks.append('AA52AA')

	def send_calib_confirmation(self):
		"""
		Called when the Calibration button is pressed again
		"""
		self.outgoing_pks.append('AA62AA')

	def send_mode(self, mode: int):
		"""
		Called when a mode button is clicked
		Mode 0 : Idle
		Mode 1 : Build
		Mode 2 : Unbuild
		"""
		match mode:
			case 1:
				buff = "AA212AA"
			case 2:
				buff = "AA222AA"
			case _:
				buff = "AA201AA"
		self.outgoing_pks.append(buff)

	def send_reset(self):
		"""
		Called when the RESET button is clicked
		Will reset both the local memory and that of the Arduino, as well as home the Dobots
		"""
		self.outgoing_pks.append("AAE3AA")

	def get_ack(self) -> bool:
		"""
		Wait for an acknowledgment from the Arduino board that a packet has been processed
		"""
		if self.running and self.online:
			try:
				ack = self.bluetooth_serial.read_until(size=7).decode('utf-8')
				if ack == '':
					raise serial.serialutil.SerialTimeoutException(self.bluetooth_serial)
				return ack == "AAF15AA"
			except serial.serialutil.SerialException:
				self.online = False
				self.bluetooth_serial.close()
				print("Timed out")
		return False

