#!/bin/sh
west build --build-dir build . --pristine --board promicro_nrf52840/nrf52840/uf2 -- -DCONF_FILE=prj-llvm.conf --toolchain ${ZEPHYR_SDK_CMAKE_TOOLCHAIN_LLVM_PICO}
