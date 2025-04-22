#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(EXTRA_DTC_OVERLAY_FILE "dts.overlay")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project("Zigbee CO2 Sensor V2")

# NORDIC SDK APP START
target_sources(app PRIVATE
  src/main.cpp
  submodules/esp_generic_lib/src/lib_linked_list.cpp
)

target_include_directories(app PRIVATE include)
target_include_directories(app PRIVATE submodules/esp_generic_lib/include)
target_link_libraries(app PRIVATE c stdc++ supc++)
target_compile_options(app PUBLIC -D__cpp_concepts=202002L -Wno-builtin-macro-redefined -Wno-invalid-offsetof)
# NORDIC SDK APP END
