#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project("Zigbee CO2 Sensor V2")

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.cpp
  # src/zb/zb_main.hpp
)

target_include_directories(app PRIVATE include)
target_link_libraries(app PRIVATE c stdc++ supc++)
# NORDIC SDK APP END
