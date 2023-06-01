//
// Created by mtr1c on 12/04/2023.
// Contains some utility functions
//

#ifndef DOBOTCITYBUILDING_DOBOTCITYUTILS_H
#define DOBOTCITYBUILDING_DOBOTCITYUTILS_H

#include <stdint.h>
#include <stdlib.h>

/**
 * Convert a hex character to an integer
 * @param hex char to convert
 * @return
 */
uint8_t Hex2Int(char hex)
{
	if(hex >= '0' && hex <= '9') return (uint8_t) (hex - '0');
	if(hex >= 'A' && hex <= 'F') return 10 + hex - 'A';
	if(hex >= 'a' && hex <= 'f') return 10 + hex - 'a';
	return -1;
}
/**
 * Read part of an hexadecimal char array and return its integer value as an int
 * @param payload Payload to read from
 * @param from Starting bit
 * @param length Portion length
 * @param payload_length Total length
 * @param result Int result ouput
 * @return Success or failure
 */
bool ReadHexInt(const char* payload, uint8_t from, uint8_t length, uint8_t payload_length, int* result)
{
	if(from + length > payload_length) return false;
	char fragment[length + 1];
	for(int i = 0; i < length; ++i)
		fragment[i] = payload[from + i];
	fragment[length] = '\0';
	*result = strtol(fragment, nullptr, 16);
	return true;
}

#endif //DOBOTCITYBUILDING_DOBOTCITYUTILS_H
