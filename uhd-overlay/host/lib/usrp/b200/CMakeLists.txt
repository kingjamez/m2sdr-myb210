#
# Copyright 2012-2013,2015 Ettus Research LLC
# Copyright 2018 Ettus Research, a National Instruments Company
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# This file included, use CMake directory variables
########################################################################

########################################################################
# Conditionally configure the B200 support
########################################################################
if(ENABLE_B200)
    LIBUHD_APPEND_SOURCES(
        ${CMAKE_CURRENT_SOURCE_DIR}/b200_image_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/b200_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/b200_iface.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/b200_io_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/b200_uart.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/b200_cores.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/b200_mb_eeprom.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/b200_radio_ctrl_core.cpp
        #below added by liwei  
        #${CMAKE_CURRENT_SOURCE_DIR}/xz_pcie/api_if.c
        #${CMAKE_CURRENT_SOURCE_DIR}/xz_pcie/gen_b210_img.c
        #${CMAKE_CURRENT_SOURCE_DIR}/xz_pcie/i2c_eeprom.c
        #${CMAKE_CURRENT_SOURCE_DIR}/xz_pcie/i2c_opencores.c
        #${CMAKE_CURRENT_SOURCE_DIR}/xz_pcie/pcie_if.c
        #${CMAKE_CURRENT_SOURCE_DIR}/xz_pcie/time.c
        #${CMAKE_CURRENT_SOURCE_DIR}/xz_pcie/tools.c  
    )
    
    
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(PCIE_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/x64_libpcie.a")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
    set(PCIE_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/arm_libpcie.a")
endif()
    
    
link_libraries(${PCIE_LIB_PATH})  

link_libraries(pthread) 


endif(ENABLE_B200)
