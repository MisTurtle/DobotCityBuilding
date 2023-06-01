//
// Created by mtr1c on 24/03/2023.
//

#ifndef DOBOTCITYBUILDER_BUILDINGSLOT_H
#define DOBOTCITYBUILDER_BUILDINGSLOT_H

#define UNIT_X_SIZE 25
#define UNIT_Y_SIZE 25
#define UNIT_Z_SIZE 20

#define T_NONE (-1)
#define T_HOUSE 0
#define T_BUILDING 1
#define T_CAR 2
#define T_TREE 3

/**
 * Represents a slot to be built in the city
 * The xPos and yPos coordinates are in R1'
 */
typedef struct tagBuildingSlot
{
	float xPos = .0f; float yPos = .0f; float rot = 0; int8_t type = T_NONE; // Slot real world infos (mm)
	uint8_t progress = 0; // Current progress in the building
	uint8_t dimX = 0; uint8_t dimY = 0; uint8_t dimZ = 0; // Slot dimensions
	uint16_t length = 0;

	void Init(float sX, float sY, float sRot, int8_t sType)
	{
		this->xPos = sX;
		this->yPos = sY;
		this->rot = sRot;
		this->type = sType;
		switch(this->type)
		{
			case T_TREE:
			case T_HOUSE:
				this->dimX = 1;
				this->dimY = 1;
				this->dimZ = 2;
				break;
			case T_BUILDING:
				this->dimX = 1;
				this->dimY = 2;
				this->dimZ = 3;
				break;
			case T_CAR:
				this->dimX = 1;
				this->dimY = 2;
				this->dimZ = 1;
				break;
		}
		this->length = sqrt(xPos * xPos + yPos * yPos);
	}

	void GetRealCurrentPosition(float* oX, float* oY, float* oZ)
	{ // Get the position of the block we're currently building
		// Center coordinates
		float center[2] = {this->xPos + (float) UNIT_X_SIZE/2, this->yPos + (float) UNIT_Y_SIZE};
		// Left or right block
		int coeff = this->progress % 2 == 1 ? -1 : 1;
		// Rotation : Degrees to radians
		float rotation = (90 - this->rot) * 71.0f / 4068.0f;

		*oX = center[0];
		*oY = center[1];
		if(this->dimY > 1)
		{
			// If the structure is 2 blocks wide, alternate between left and right block
			*oX -= coeff * UNIT_X_SIZE * cos(rotation) / 2.f;
			*oY += coeff * UNIT_Y_SIZE * sin(rotation) / 2.f;
		}
		// Move 1 block up every time a level is fully built
		*oZ = floor(this->progress / (this->dimX * this->dimY)) * UNIT_Z_SIZE;
	}
	uint8_t GetMaxProgress()
	{
		return this->dimX * this->dimY * this->dimZ;
	}
} BuildingSlot;

/**
 * Represents a block that was detected in the storage zone
 * Coordinates are in R1
 */
typedef struct tagBuildingCompound
{
	float xPos = .0f; float yPos = .0f; float rot = 0; int8_t type = T_NONE;

	void GetRealPosition(float* oX, float* oY, float* oRot) const
	{
		*oX = xPos;
		*oY = yPos;
		*oRot = rot;
	}
} BuildingCompound;

#endif //DOBOTCITYBUILDER_BUILDINGSLOT_H
