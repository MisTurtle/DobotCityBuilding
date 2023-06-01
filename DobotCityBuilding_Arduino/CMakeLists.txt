cmake_minimum_required(VERSION 3.23)
project(DobotCityBuilding)

set(CMAKE_CXX_STANDARD 14)

include_directories(. ../DobotNet/src/)

add_executable(DobotCityBuilding BuildingSlot.h CityMapHandler.h
        DobotCityBuilding.ino keep.cpp DobotBuilderUnit.h DobotCityUtils.h BluetoothHandler.h)
