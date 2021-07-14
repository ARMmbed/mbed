# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

### J-Link Upload Method
# This method needs the following parameters:
# JLINK_UPLOAD_INTERFACE - Interface to use with J-Link.  Should be "JTAG" or "SWD".
# JLINK_CLOCK_SPEED - Speed to run the J-Link at, in KHz.
# JLINK_CPU_NAME - Name of CPU to pass to J-Link

set(UPLOAD_SUPPORTS_DEBUG TRUE)

### Check if upload method can be enabled on this machine
find_package(JLINK)
set(UPLOAD_JLINK_FOUND ${JLINK_FOUND})

# default to JTAG
if(NOT DEFINED JLINK_UPLOAD_INTERFACE)
	set(JLINK_UPLOAD_INTERFACE JTAG)
endif()

### Function to generate upload target
function(gen_upload_target TARGET_NAME BIN_FILE HEX_FILE)

	# create command file for j-link
	set(COMMAND_FILE_PATH ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/flash-${TARGET_NAME}.jlink)
	file(GENERATE OUTPUT ${COMMAND_FILE_PATH} CONTENT
"loadfile ${HEX_FILE}
r
exit
")
	add_custom_target(flash-${TARGET_NAME}
		COMMENT "Flashing ${TARGET_NAME} with J-Link..."
		COMMAND ${JLINK}
		-Device ${JLINK_CPU_NAME}
		-Speed ${JLINK_CLOCK_SPEED}
		-if ${JLINK_UPLOAD_INTERFACE}
		-JTAGConf -1,-1
		-AutoConnect 1
		-ExitOnError
		-CommandFile ${COMMAND_FILE_PATH})


	add_dependencies(flash-${TARGET_NAME} ${TARGET_NAME})

endfunction(gen_upload_target)

### Function to generate debug target
add_custom_target(gdbserver
	COMMENT "Starting J-Link GDB server"
	COMMAND
	"${JLINK_GDBSERVER}"
	-Select USB
	-Device ${JLINK_CPU_NAME}
	-Speed ${JLINK_CLOCK_SPEED}
	-endian little
	-if ${JLINK_UPLOAD_INTERFACE}
	-JTAGConf -1,-1
	-LocalhostOnly
	-noIR
	-port ${GDB_PORT}
	USES_TERMINAL)