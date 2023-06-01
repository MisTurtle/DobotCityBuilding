//
// Created by mtr1c on 12/04/2023.
//

#ifndef DOBOTCITYBUILDING_DOBOTBUILDERUNIT_H
#define DOBOTCITYBUILDING_DOBOTBUILDERUNIT_H

#define PARAM_CALIBRATED 0
#define PARAM_CALIBRATION_REQUESTED 1

#include "DobotInstance.h"

class DobotBuilderUnit: public DobotInstance
{
public:
	DobotBuilderUnit(SerialInterface* sPort, uint8_t id): DobotInstance(sPort, id)
	{
		this->SetCalibrated(false);
	}
	void SendBasePackets();
	bool IsCalibrated();
	bool SetCalibrated(bool value);
};

void DobotBuilderUnit::SendBasePackets()
{
	// IO Multiplexing : EIO18 as regular output
	this->ClearAlarm(); // Clear any alarm state
	this->SetIOMultiplexing(18, REG_OUTPUT, true); // EIO18 as 3.3V output
	this->SetIODO(18, 1, true); // EIO18 high, set as working
	this->SerialProcess(nullptr); // Flush

	// Set end effector to be the suction cup
	this->SetEndEffectorSuctionCup(true, false, true);
	this->SetEndEffectorParams(59.7, 0, 0, true);
	this->SerialProcess(nullptr); // Flush

	// Set PTP Speed and Acceleration values
	this->SetPTPJumpParams(50, 90, true); // Jumping params
	this->SetPTPCoordinateParams(100, 100, 80, 80, true); // XYZ moving speed
	this->SetPTPCommonParams(50, 50, true); // Ratios
	this->SerialProcess(nullptr); // Flush

	// Set HOME Params, HOME, and remove the working state
	this->SetHomeParams(200, 0, 0, 0, true);
	this->DoHomeProcedure(true);
	this->SetIODO(18, 0, true);
	this->SerialProcess(nullptr); // Flush
}

bool DobotBuilderUnit::IsCalibrated()
{
	return this->GetParam(PARAM_CALIBRATED);
}

bool DobotBuilderUnit::SetCalibrated(bool value)
{
	this->SetParam(PARAM_CALIBRATED, value);
}

#endif //DOBOTCITYBUILDING_DOBOTBUILDERUNIT_H
