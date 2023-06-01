#include "DobotNet.h"
#include "DobotBuilderUnit.h"
#include "CityMapHandler.h"
#include "BluetoothHandler.h"
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>

#define WAIT_CAUSE_CALIBRATION 1
#define WAIT_CAUSE_IDLING 2
#define WAIT_CAUSE_WAITING 3
#define WAIT_CAUSE_WORKING 4
#define WAIT_CAUSE_JOB_DONE 5
#define WAIT_CAUSE_BLUETOOTH 6
#define WAIT_CAUSE_NO_BLOCK 7
#define WAIT_CAUSE_CALIBRATION_REQUESTED 8
#define WAIT_CAUSE_NONE 9


// Emulate UART on pins RX: 64, TX: 65 for Bluetooth
SoftwareSerial S2{64, 65};
// Emulate UART on pins RX: 10, TX: 11 for Dobot
// TODO SoftwareSerial S3{10, 11};

// Bluetooth Module is on RX: 64, TX: 65
SoftwareSerialWrapper bluetoothS0{&S2};
// Storage Dobot is on Serial 1
HardwareSerialWrapper dobotS1{&Serial1};
// Builder Dobot is on Serial 2
HardwareSerialWrapper dobotS2{&Serial2};

// Create Dobot Instances
DobotBuilderUnit dobots[2] = {{&dobotS1, 0}, {&dobotS2, 1}};
uint8_t dobotOutputPins[2] = {2, 3};


// Dobot Offsets
float offset[2][3] = {{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}}; // Coordinates when dobots hover the calibration point
float calibrationPoint[2][2] = {{180.f, 181.f}, {180.5f, 209.f}}; // Calibration point coordinates in mm
float transitionSlot[2][2] = {{126.f, -36.f}, {126.f, 384.f}}; // Transition slot coordinates in mm
float computedTransitionSlot[2][2] = {{0.f, 0.f}, {0.f, 0.f}}; // Transition slot coordinates in dobot coordinates

// Dobot Response Callbacks
void Handle_DobotRx(uint8_t dobotId, Message* msg);

DobotResponseCallback cbs[2] = {Handle_DobotRx, Handle_DobotRx};

// Module Handlers
CityMapHandler mapHandler;
BluetoothHandler bluetoothHandler;

// Waiting for a dobot
bool waiting = false; // Are we waiting at all
bool waitingState = false; // State that we're waiting for
int8_t waitingDobotId = 0; // ID of the dobot that should get to this state

// Previous state
uint8_t prev_waiting_reason;
uint8_t prev_waiting_extra;

LiquidCrystal_I2C lcd(0x27, 4, 20);

// Compute millimeters coordinates to dobot-based coordinates
void compute_dobot_coordinates(int dobotId, float *x, float *y, float *z);

// Dobot working state handling
void set_dobot_working(int dobotId, bool working, bool queued);
bool is_dobot_working(int dobotId);
bool any_dobot_working();
void wait_until_working(int8_t dobotId, bool working = true);

// Dobot City Building Procedure Main Functions
bool PerformStorageStep(DobotBuilderUnit* bot);
bool PerformBuildingStep(DobotBuilderUnit* bot);

void perform_delay(uint8_t cause, uint8_t extra = 0, bool force_reload = false);

void setup()
{
	delay(1000);

	// Init and clear LCD screen
	lcd.init();
	lcd.backlight();

	// Start Serial Monitor @ 9600 bauds
	Serial.begin(9600);
	Serial.println("\n\n\n\n===[DobotCityBuilder Procedure]===\n\n\n\n");

	// Initialize the map
	Serial.print("Initializing City Map Data...");
	mapHandler.Init();
	Serial.println(" Done !");

	// Initialize the bluetooth module
	Serial.print("Initializing Bluetooth Module...");
	bluetoothHandler.Init(&bluetoothS0);
	Serial.println(" Done !");

	Serial.println("Initializing Dobot Net...");

	for(auto & pin : dobotOutputPins) pinMode(pin, INPUT);
	DobotNet::Init(dobots, 2);

	dobots[0].SendBasePackets();
	wait_until_working(0, true);
	wait_until_working(0, false);
	dobots[1].SendBasePackets();

	Serial.println(" Done!");

	bluetoothS0.begin(9600);
}

void loop()
{
	bluetoothHandler.Tick();

	DobotNet::Tick(cbs);

	auto bStorage = reinterpret_cast<DobotBuilderUnit*>(DobotNet::GetDobot(0));
	auto bBuilder = reinterpret_cast<DobotBuilderUnit*>(DobotNet::GetDobot(1));

#if 1
	if(bBuilder->GetParam(PARAM_CALIBRATION_REQUESTED) || bStorage->GetParam(PARAM_CALIBRATION_REQUESTED))
		return perform_delay(WAIT_CAUSE_CALIBRATION_REQUESTED);
	if(!bStorage->IsCalibrated())
		return perform_delay(WAIT_CAUSE_CALIBRATION, 0);
	if(!bBuilder->IsCalibrated())
		return perform_delay(WAIT_CAUSE_CALIBRATION, 1);
	if(CityMapHandler::instance->mode == 0)
		return perform_delay(WAIT_CAUSE_IDLING);
	waiting = (waiting && (waitingState ^ is_dobot_working(waitingDobotId)));
	if(waiting)
		return perform_delay(WAIT_CAUSE_WAITING);
	if(any_dobot_working())
		return perform_delay(WAIT_CAUSE_WORKING);
	if(mapHandler.IsFinishedBuilding())
		return perform_delay(WAIT_CAUSE_JOB_DONE);
	if(millis() - bluetoothHandler.last_rcv_ts > BLUETOOTH_TIMEOUT)
		return perform_delay(WAIT_CAUSE_BLUETOOTH);

	bool ret;

	if(mapHandler.GetTransitionSlotContent() == T_NONE) ret = PerformStorageStep(bStorage);
	else ret = PerformBuildingStep(bBuilder);

	if(ret) perform_delay(WAIT_CAUSE_NONE, 0, true);

#else
	if(any_dobot_working()) return perform_delay("Working");
	PerformBuildingStep(bStorage);
#endif
}


// IMPLEMENTATIONS
void compute_dobot_coordinates(int dobotId, float *x, float *y, float *z)
{
	*x = *x - calibrationPoint[dobotId][0] + offset[dobotId][0];
	*y = -*y + calibrationPoint[dobotId][1] + offset[dobotId][1];
	*z = *z + UNIT_Z_SIZE - 5;
}

void set_dobot_working(int dobotId, bool working, bool queued)
{
	if(dobotId < 0 || dobotId > 1) return;
	DobotNet::GetDobot(dobotId)->SetIODO(18, working, queued);
}
bool is_dobot_working(int dobotId)
{
	return dobotId >= 0 && dobotId <= 1 && digitalRead(dobotOutputPins[dobotId]) == HIGH;
}
bool any_dobot_working()
{
	return digitalRead(dobotOutputPins[0]) == HIGH || digitalRead(dobotOutputPins[1]) == HIGH;
}
void wait_until_working(int8_t dobotId, bool working)
{
	waiting = true;
	waitingDobotId = dobotId;
	waitingState = working;
}

bool PerformStorageStep(DobotBuilderUnit* bot)
{
	auto target = mapHandler.GetTargetCompound();
	if(target == nullptr)
	{
		perform_delay(WAIT_CAUSE_NO_BLOCK, mapHandler.GetTargetSlot()->type);
		return false;
	}

	float x, y, z, rot;
	target->GetRealPosition(&x, &y, &rot);
	z = offset[0][2];
	compute_dobot_coordinates(0, &x, &y, &z);

	// Go fetch the block
	set_dobot_working(0, true, false);
	bot->SetGripping(false, true);
	bot->MoveTo(JUMP_XYZ, x, y, z, -rot);
	bot->SetGripping(true, true);
	bot->SerialProcess(nullptr);

	// Go place it
	bot->Delay(500, true);
	bot->MoveTo(JUMP_XYZ, computedTransitionSlot[0][0], computedTransitionSlot[0][1], z, 0);
	bot->SetGripping(false, true);
	bot->Delay(500, true);
	bot->SerialProcess(nullptr);

	// Go to idle pos
	bot->DisableGripper(true);
	bot->MoveTo(JUMP_XYZ, 225, -180, offset[0][2] + 5, 0);
	set_dobot_working(0, false, true);
	bot->SerialProcess(nullptr);

	// Wait for the dobot to start working (otherwise the builder dobot might start its procedure right away)
	wait_until_working(0, true);
	mapHandler.SetTransitionSlotContent(target->type);
}

bool PerformBuildingStep(DobotBuilderUnit* bot)
{
	auto target = mapHandler.GetTargetSlot();
	if(target == nullptr)
	{
		perform_delay(WAIT_CAUSE_JOB_DONE);
		return false;
	}

	float x, y, z;
	target->GetRealCurrentPosition(&x, &y, &z);
	compute_dobot_coordinates(1, &x, &y, &z);

	// Fetch the block
	set_dobot_working(1, true, false);
	bot->SetGripping(false, true);
	bot->MoveTo(JUMP_XYZ, computedTransitionSlot[1][0], computedTransitionSlot[1][1], offset[1][2] + UNIT_Z_SIZE - 5, 0);
	bot->SetGripping(true, true);
	bot->SerialProcess(nullptr);

	if(y > 100) bot->MoveTo(MOVL_XYZ, computedTransitionSlot[1][0] + 50, computedTransitionSlot[1][1], 2 * UNIT_Z_SIZE, 0);

	// Go place it
	bot->MoveTo(JUMP_XYZ, x, y, offset[1][2] + z + UNIT_Z_SIZE - 20, -target->rot + 90);
	bot->SetGripping(false, true);
	bot->Delay(500, true);
	bot->SerialProcess(nullptr);

	bot->DisableGripper(true);
	// Come back to idle pos
	bot->MoveTo(JUMP_XYZ, 225, 0, 0, 0);
	set_dobot_working(1, false, true);
	bot->SerialProcess(nullptr);

	mapHandler.ProgressConfirmed();

	wait_until_working(1, true);
	mapHandler.SetTransitionSlotContent(T_NONE);

	return true;
}

void perform_delay(uint8_t cause, uint8_t extra, bool force_reload)
{
	if(!force_reload && prev_waiting_reason == cause && prev_waiting_extra == extra) return; // Same text as before

	prev_waiting_reason = cause;
	prev_waiting_extra = extra;
	lcd.clear();
	switch (cause)
	{
		case WAIT_CAUSE_CALIBRATION_REQUESTED:
			lcd.setCursor(0, 0);
			lcd.print("Status: Waiting");
			lcd.setCursor(0, 1);
			lcd.print("Cause: Calibration");
			lcd.setCursor(0, 2);
			lcd.print("Place the dobots end");
			lcd.setCursor(0, 3);
			lcd.print("on the red dot");
			break;
		case WAIT_CAUSE_CALIBRATION:
			lcd.setCursor(0, 0);
			lcd.print("Status: Waiting");
			lcd.setCursor(0, 1);
			lcd.print("Cause: Calibration");
			lcd.setCursor(0, 2);
			lcd.print("Reason: Dobot ");
			if(extra == 0)
			{
				lcd.print("0 is");
				Serial.println("Storage Dobot is not calibrated");
			}else{
				lcd.print("1 is");
				Serial.println("Builder Dobot is not calibrated");
			}
			lcd.setCursor(0, 3);
			lcd.print("not calibrated");
			break;
		case WAIT_CAUSE_IDLING:
			lcd.setCursor(0, 0);
			lcd.print("Status: Idling");
			lcd.setCursor(0, 1);
			lcd.print("Cause: Idling mode");
			lcd.setCursor(1, 3);
			lcd.print("---[ DobotNet ]---");

			Serial.println("Idling mode");
			break;
		case WAIT_CAUSE_WAITING:
			lcd.setCursor(0, 0);
			lcd.print("Status: Waiting");
			lcd.setCursor(0, 1);
			lcd.print("Cause: Dobot Signal");
			lcd.setCursor(0, 2);
			lcd.print("Subject: ");
			lcd.print(waitingDobotId);

			Serial.println("Waiting for a dobot to be flagged as working");
			break;
		case WAIT_CAUSE_WORKING:
			int progress, total;
			mapHandler.GetBuildingState(&progress, &total);

			lcd.setCursor(0, 0);
			lcd.print("Status: Working");
			lcd.setCursor(0, 1);
			lcd.print("Mode: Building");
			lcd.setCursor(0, 2);
			lcd.print("Active: ");
			lcd.print(is_dobot_working(0) ? "0" : "1");
			lcd.setCursor(0, 3);
			lcd.print("Progress: ");
			lcd.print(progress);
			lcd.print("/");
			lcd.print(total);

			Serial.println("Someone is working");
			break;
		case WAIT_CAUSE_JOB_DONE:
			lcd.setCursor(0, 0);
			lcd.print("Status: Done");
			lcd.setCursor(0, 1);
			lcd.print("Mode: Building");
			lcd.setCursor(0, 2);
			lcd.print("Active: None");
			lcd.setCursor(0, 3);
			lcd.print("Progress: DONE !");

			Serial.println("Procedure Finished");
			break;
		case WAIT_CAUSE_BLUETOOTH:
			lcd.setCursor(0, 0);
			lcd.print("Status: Error");
			lcd.setCursor(0, 1);
			lcd.print("Cause: Bluetooth");
			lcd.setCursor(0, 2);
			lcd.print("Reason: Silent for");
			lcd.setCursor(0, 3);
			lcd.print("more than 5 sec");

			Serial.println("Bluetooth flow has been silent for 5+ seconds");
			break;
		case WAIT_CAUSE_NO_BLOCK:
			lcd.setCursor(0, 0);
			lcd.print("Status: Waiting");
			lcd.setCursor(0, 1);
			lcd.print("Cause: Supply");
			lcd.setCursor(0, 2);
			lcd.print("No ");
			switch(extra)
			{
				case T_HOUSE:
					lcd.print("HOUSE");
					break;
				case T_CAR:
					lcd.print("CAR");
					break;
				case T_BUILDING:
					lcd.print("BUILDING");
					break;
				case T_TREE:
					lcd.print("TREE");
					break;
				default:
					lcd.print(" ??? ");
					break;
			}
			lcd.print(" block");
			lcd.setCursor(0, 3);
			lcd.print("was found");

			Serial.println("No block in the storage zone matches the current requirements");
			break;
		default:
			break;
	}
	delay(100);
}

void Handle_DobotRx(uint8_t dobotId, Message* msg)
{
	auto bot = reinterpret_cast<DobotBuilderUnit*>(DobotNet::GetDobot(dobotId));

	if(bot != nullptr)
	{
		switch(msg->id)
		{
			case ProtocolGetPose:
				// In our case, this shouldn't need to be called except if the dobot isn't calibrated yet
				printf("\n\n\n\n\nRECEIVED DOBOT POSE\n\n\n\n\n");


				Pose pose{}; // Struct to host the Message data

				// Deserialize packet buffer
				memcpy(&pose, msg->params, sizeof(Pose));
				// Set offset
				offset[dobotId][0] = pose.x;
				offset[dobotId][1] = pose.y;
				offset[dobotId][2] = pose.z;

				Serial.println(pose.x);
				Serial.println(pose.y);
				Serial.println(pose.z);

//				computedTransitionSlot[dobotId][0] = transitionSlot[dobotId][0] - calibrationPoint[dobotId][0] + pose.x;
//				computedTransitionSlot[dobotId][1] = -transitionSlot[dobotId][1] + calibrationPoint[dobotId][1] - pose.y;

				computedTransitionSlot[dobotId][0] = transitionSlot[dobotId][0] - calibrationPoint[dobotId][0] + pose.x;
				computedTransitionSlot[dobotId][1] = -(transitionSlot[dobotId][1] - calibrationPoint[dobotId][1] - pose.y);

				bot->SetCalibrated(true);
				break;
		}
	}
}
