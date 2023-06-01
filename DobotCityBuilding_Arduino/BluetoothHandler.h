//
// Created by mtr1c on 12/04/2023.
//

#ifndef DOBOTCITYBUILDING_BLUETOOTHHANDLER_H
#define DOBOTCITYBUILDING_BLUETOOTHHANDLER_H

// Packet Format :
// AA - X - AA
// Header - Payload - Footer

// Payload Format :
// 1st byte => Command ID
// Then, each parameter has a preset size
// Last byte is the checksum (Sum of all the bits INSIDE the Payload, including the command ID), mod 10

// EVERY PARAMETER IS PASSED AS AN HEXADECIMAL INTEGER

#include "SerialInterface.h"
#include "DobotCityUtils.h"
#include "CityMapHandler.h"
#include "DobotNet.h"
#include "DobotBuilderUnit.h"
#include <stdint.h>

#define BLUETOOTH_TIMEOUT 5000

// Example : AA00AA
#define CMD_RESET_BLOCKS 0

// Parameters : xPos (4bytes), yPos (4bytes), rot(4bytes), type(1byte)
// xPos and yPos are divided by 100 once they're received (Multiplied by 100 to be passed as an integer)
// Example : AA10A5C0473013316AA
#define CMD_SET_BLOCK 1

// Parameters : mode (1 byte)
// Set the mode to operate
// Example : AA212AA
#define CMD_SET_MODE 2

// Parameters : None
// Example : AA52AA
#define CMD_REQUEST_CALIBRATION 5

// Parameters : None
// Example : AA62AA
#define CMD_CONFIRM_CALIBRATION 6

// Parameters : None
// Example : AAE3AA
#define CMD_RESET 14

// Parameters : Valid (1 byte)
// Example : AAF15AA
#define CMD_ACK 15

#define MAX_PAYLOAD_LENGTH 14
#define MIN_PAYLOAD_SIZE 6
#define MAX_PACKET_LENGTH 20
#define MAX_PACKET_UNIQUE_IDS 16 // 0 to F <=> 0 to 15

typedef bool(*BluetoothHandlerFunc)(const char*, uint8_t);


// BLUETOOTH HANDLER FUNCTIONS //
bool Handle_ResetBlocks(const char payload[MAX_PACKET_LENGTH], uint8_t payload_length)
{
	CityMapHandler::instance->ResetCompounds();
	printf("Cleared Compounds");
	return true;
}
bool Handle_SetBlock(const char payload[MAX_PACKET_LENGTH], uint8_t payload_length)
{
	int x, y, rot, type;
	if(
			payload_length != 19 ||
			!ReadHexInt(payload, 3, 4, payload_length, &x) ||
			!ReadHexInt(payload, 7, 4, payload_length, &y) ||
			!ReadHexInt(payload, 11, 4, payload_length, &rot) ||
			!ReadHexInt(payload, 15, 1, payload_length, &type)
			) return false;

	float xf = (float) x / 100.0f;
	float yf = (float) y / 100.0f;
	float rotf = (float) rot / 100.0f;
	return CityMapHandler::instance->CreateCompound(xf, yf, rotf, type);
}

bool Handle_SetMode(const char payload[MAX_PACKET_LENGTH], uint8_t payload_length)
{
	int mode;
	if(payload_length != 7 || !ReadHexInt(payload, 3, 1, payload_length, &mode)) return false;
	CityMapHandler::instance->mode = mode;
  printf("\n\nSet mode to %d\n\n", mode);
	return true;
}

bool Handle_CalibRequest(const char payload[MAX_PACKET_LENGTH], uint8_t payload_length)
{
	for(int i = 0; i < 2; ++i)
	{
		auto dobot = reinterpret_cast<DobotBuilderUnit*>(DobotNet::GetDobot(i));
		dobot->SetCalibrated(false);
		dobot->SetParam(PARAM_CALIBRATION_REQUESTED, true);
	}
	printf("\n\nCalibration Requested.\n\n");
	return true;
}
bool Handle_CalibConfirm(const char payload[MAX_PACKET_LENGTH], uint8_t payload_length)
{
	for(int i = 0; i < 2; ++i)
	{
		auto dobot = reinterpret_cast<DobotBuilderUnit*>(DobotNet::GetDobot(i));
		dobot->SendGetterProtocol(ProtocolGetPose, false);
		dobot->SetParam(PARAM_CALIBRATION_REQUESTED, false);
	}
	printf("\n\nCalibration Confirmed.\n\n");
	return true;
}
bool Handle_Reset(const char payload[MAX_PACKET_LENGTH], uint8_t payload_length)
{
	for(int i = 0; i < 2; ++i)
	{
		auto dobot = reinterpret_cast<DobotBuilderUnit*>(DobotNet::GetDobot(i));

		dobot->SetCalibrated(false);
		dobot->SetParam(PARAM_CALIBRATION_REQUESTED, false);
		dobot->SendBasePackets();

		CityMapHandler::instance->Reset();
	}
	printf("\n\nEVERYTHING IN MEMORY WAS RESET\n\n");
	return true;
}
bool Handle_Ack(const char payload[MAX_PACKET_LENGTH], uint8_t payload_length)
{
	// When received, an acknowledgment packet is just a check to inform the control interface
	// that the bluetooth module is still online
	printf("\n\nPresence Confirmed\n\n");
	return true;
}
// END : BLUETOOTH HANDLER FUNCTIONS //


class BluetoothHandler
{
public:
	// Base
	void Init(SerialInterface* _serial);
	SerialInterface* GetSerialPort();

	// Tick function, called on every loop call
	void Tick();

	// Payload Handling
	bool IsPayloadComplete();
	void ClearPayload();
	bool ValidateChecksum();

	// Communication
	void Acknowledge(bool valid);

	// Handler function management
	BluetoothHandlerFunc GetHandler(uint8_t pid);
	void SetHandler(uint8_t pid, BluetoothHandlerFunc handler);

	unsigned long last_rcv_ts = 0; // Last time we received data through bluetooth
private:
	void ThrowPayloadError(const char* Str);

	SerialInterface* serialPort = nullptr;
	char payload[MAX_PACKET_LENGTH] = {};
	uint8_t payload_length = 0;

	BluetoothHandlerFunc handlers[MAX_PACKET_UNIQUE_IDS] = {};
};

void BluetoothHandler::Init(SerialInterface *_serial)
{
	this->serialPort = _serial;
//	this->serialPort->begin(9600); // somehow doesn't work

	for(auto & handler : this->handlers) handler = nullptr;

	// Register Handlers
	this->SetHandler(CMD_RESET_BLOCKS, &Handle_ResetBlocks);
	this->SetHandler(CMD_SET_BLOCK, &Handle_SetBlock);
	// ...
	this->SetHandler(CMD_SET_MODE, &Handle_SetMode);
	this->SetHandler(CMD_REQUEST_CALIBRATION, &Handle_CalibRequest);
	this->SetHandler(CMD_CONFIRM_CALIBRATION, &Handle_CalibConfirm);
	// ...
	this->SetHandler(CMD_RESET, &Handle_Reset);
	this->SetHandler(CMD_ACK, &Handle_Ack);

	this->ClearPayload();
}

SerialInterface* BluetoothHandler::GetSerialPort()
{
	return this->serialPort;
}

void BluetoothHandler::Tick()
{
	// Get incoming bytes from bluetooth module
	while(this->serialPort->available())
	{

		//Serial.print("\nIncoming Bluetooth Data : ");
		while(this->serialPort->available() > 0 && this->payload_length < MAX_PACKET_LENGTH && !this->IsPayloadComplete())
		{
			this->payload[(this->payload_length)++] = (char) this->serialPort->read();
			//Serial.print(this->payload[this->payload_length - 1]);
		}
		//Serial.print("\n");

		if(this->payload_length >= MAX_PACKET_LENGTH) return this->ThrowPayloadError("Invalid Payload");

		if(!this->IsPayloadComplete()) return;

		if(!this->ValidateChecksum()) return this->ThrowPayloadError("Invalid Checksum");

		// Process payload
		int pid = Hex2Int(this->payload[2]);

		if(pid < 0 || pid >= MAX_PACKET_UNIQUE_IDS) return this->ThrowPayloadError("Invalid Packet Id");

		if(this->handlers[pid] == nullptr) return this->ThrowPayloadError("Unhandled Packet Id");

		if(!this->handlers[pid](this->payload, this->payload_length)) return this->ThrowPayloadError("Failed to process packet");

		this->last_rcv_ts = millis();
		this->Acknowledge(true);
		this->ClearPayload();
	}
}

bool BluetoothHandler::IsPayloadComplete()
{

	if(this->payload_length < MIN_PAYLOAD_SIZE)  // 4 embed bytes AA - AA, 1 for packet id, 1 for checksum
	{
		if((this->payload_length >= 1 && this->payload[0] != 'A') || this->payload_length >= 2 && this->payload[1] != 'A')
			this->ClearPayload();
		return false;
	}
	if(this->payload[0] != 'A' || this->payload[1] != 'A')
	{
		this->ClearPayload();
		return false;
	}
	if(this->payload[payload_length - 2] != 'A' || this->payload[payload_length - 1] != 'A') return false;
	return true;
}

void BluetoothHandler::ClearPayload()
{
	for(int i = 0; i < this->payload_length; ++i)
		this->payload[i] = '\0';
	this->payload_length = 0;
}

bool BluetoothHandler::ValidateChecksum()
{
	uint8_t checksum = 0;
	for(uint8_t i = 2; i < this->payload_length - 3; ++i)
	{
		uint8_t chr = Hex2Int(this->payload[i]);
		for(uint8_t bit_n = 0; bit_n < 8; ++bit_n)
			checksum += (chr >> bit_n) & 0x01;
	}
	return (checksum % 10) == (this->payload[this->payload_length - 3] - '0');
}

void BluetoothHandler::Acknowledge(bool valid)
{
	this->serialPort->write(valid ? "AAF15AA" : "AAF04AA");
}

void BluetoothHandler::ThrowPayloadError(const char* Str)
{
	Serial.println(Str);
	this->ClearPayload();
	this->Acknowledge(false);
}

BluetoothHandlerFunc BluetoothHandler::GetHandler(uint8_t pid)
{
	if(pid >= MAX_PACKET_UNIQUE_IDS) return nullptr;
	return this->handlers[pid];
}

void BluetoothHandler::SetHandler(uint8_t pid, BluetoothHandlerFunc handler)
{
	if(pid < MAX_PACKET_UNIQUE_IDS) this->handlers[pid] = handler;
}

#endif //DOBOTCITYBUILDING_BLUETOOTHHANDLER_H
