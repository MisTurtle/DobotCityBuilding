//
// Created by mtr1c on 24/03/2023.
//

#ifndef DOBOTCITYBUILDER_CITYMAPHANDLER_H
#define DOBOTCITYBUILDER_CITYMAPHANDLER_H

#define BUILDING_COUNT 10
#define COMPOUND_COUNT 28

#include "BuildingSlot.h"

class CityMapHandler
{
public:
	static CityMapHandler* instance;
	void Init(); // Called to init the city
	BuildingSlot* GetTargetSlot(); // What slot are we currently working on
	int8_t GetTransitionSlotContent(); // What's the content of the transition slot
	void SetTransitionSlotContent(int8_t content); // What's the content of the transition slot

	// Called when the builder dobot has confirmed a tile has been placed
	// Either go to the next step in the current slot or switch to the next slot if any
	void ProgressConfirmed();
	bool IsFinishedBuilding();
	BuildingCompound* GetTargetCompound();

	void ResetCompounds();
	bool CreateCompound(float xPos, float yPos, float rot, uint8_t type);

	void GetBuildingState(int* progress, int* citySize);
	void Reset();

	uint8_t mode = 0; // Mode currently being operated (0 = Idle, 1 = Build, 2 = Unbuild)

private:
	BuildingSlot _slots[BUILDING_COUNT]; // Every slot that need to be built upon
	BuildingCompound _compounds[COMPOUND_COUNT]; // Building compounds that have last been read by the camera
	uint8_t _compoundCount = 0; // How many compounds have been sent by the laptop
	int8_t _transitionSlot = T_NONE; // State of the transition slot
	uint8_t _currentSlot = 0; // ID of the slot currently being worked on
	uint8_t _totalProgress = 0; // How many blocks have been place in the city
};

CityMapHandler* CityMapHandler::instance = nullptr;

void CityMapHandler::Init()
{ // Register the structures to be built on the map
	CityMapHandler::instance = this;

	this->_slots[0].Init(206.5, 120, 0.f, T_TREE);
	this->_slots[1].Init(215.5, 187.5, -52.63f, T_CAR);
	this->_slots[2].Init(137.25, 112.6, -30.f, T_CAR);
	this->_slots[3].Init(169.5, 51.2, -30.f, T_BUILDING);
	this->_slots[4].Init(156, 217, -45.f, T_BUILDING);
	this->_slots[5].Init(47.3, 10, -15.f, T_HOUSE);
	this->_slots[6].Init(218.7, 267, 30.f, T_HOUSE);
	this->_slots[7].Init(136, 288, 15.f, T_TREE);
	this->_slots[8].Init(190.7, 317.7, 0.f, T_TREE);
  this->_slots[9].Init(98.6, 25.2, -15.f, T_CAR);
	
#if 1
  // Order them by length from the origin of R1'
	for(int i = 0; i < BUILDING_COUNT; ++i)
	{
		for(int j = i + 1; j < BUILDING_COUNT; ++j)
		{
			if(_slots[i].length > _slots[j].length)
			{
				auto temp = this->_slots[i];
				this->_slots[i] = this->_slots[j];
				this->_slots[j] = temp;
			}
		}
	}
#endif
}

/**
 * @return The structure we're currently building, or nullptr if done building
 */
BuildingSlot* CityMapHandler::GetTargetSlot()
{
	if(this->_currentSlot >= BUILDING_COUNT) return nullptr;
	return this->_slots + this->_currentSlot;
}
/**
 * @return The kind of block is inside the transition slot
 */
int8_t CityMapHandler::GetTransitionSlotContent()
{
	return this->_transitionSlot;
}
/**
 * Set the transition slot content
 * @param content
 */
void CityMapHandler::SetTransitionSlotContent(int8_t content)
{
	this->_transitionSlot = content;
}

/**
 * Confirm that one block has been placed inside the map
 */
void CityMapHandler::ProgressConfirmed()
{
	if(this->IsFinishedBuilding()) return;
	++this->_totalProgress;
	auto slot = this->GetTargetSlot();
	if(++slot->progress >= slot->GetMaxProgress())
		++this->_currentSlot;
}
/**
 * @return Is the map is done building
 */
bool CityMapHandler::IsFinishedBuilding()
{
	if(this->_currentSlot >= BUILDING_COUNT) return true;
	if(this->_currentSlot != BUILDING_COUNT - 1) return false;
	auto slot = this->GetTargetSlot();
	return slot->progress >= slot->GetMaxProgress();
}

void CityMapHandler::ResetCompounds()
{
	this->_compoundCount = 0;
}
/**
 * Register a compound detected in the storage zone
 * @param xPos
 * @param yPos
 * @param rot
 * @param type
 * @return
 */
bool CityMapHandler::CreateCompound(float xPos, float yPos, float rot, uint8_t type)
{
	if(this->_compoundCount >= COMPOUND_COUNT) return false;

	this->_compounds[this->_compoundCount].xPos = xPos;
	this->_compounds[this->_compoundCount].yPos = yPos;
	this->_compounds[this->_compoundCount].rot = rot;
	this->_compounds[this->_compoundCount].type = type;
	++this->_compoundCount;

	Serial.print("\n\nNew Compound registered (x=");
	Serial.print(xPos);
	Serial.print(", y=");
	Serial.print(yPos);
	Serial.print(", rot=");
	Serial.print(rot);
	Serial.print(", type=");
	Serial.print(type);
  Serial.println("\n");

	return true;
}

/**
 * @return Which block we should fetch from the storage zone
 */
BuildingCompound* CityMapHandler::GetTargetCompound()
{
	if(this->IsFinishedBuilding()) return nullptr;
	uint8_t targetType = this->GetTargetSlot()->type;
	if(targetType == T_NONE) return nullptr;

	for(int i = 0; i < this->_compoundCount; ++i)
	{
		if(this->_compounds[i].type == targetType)
			return this->_compounds + i;
	}
	return nullptr;
}

/**
 * Get the current progress of the building of the map
 * @param progress
 * @param citySize
 */
void CityMapHandler::GetBuildingState(int *progress, int *citySize)
{
	*progress = this->_totalProgress;
	*citySize = COMPOUND_COUNT; // This will change once users can create their own city
}

/**
 * Reset the map's building progress
 */
void CityMapHandler::Reset()
{
	this->mode = 0;
	this->_compoundCount = 0;
	this->_transitionSlot = T_NONE;
	this->_currentSlot = 0;
	this->_totalProgress = 0;
	for(int i = 0; i < BUILDING_COUNT; ++i)
		this->_slots[i].progress = 0;
}

#endif //DOBOTCITYBUILDER_CITYMAPHANDLER_H
