# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

### OpenOCD Upload Method
# This method needs the following parameters:
# OPENOCD_CHIP_CONFIG_COMMANDS - Specifies all OpenOCD commands needed to configure openocd for your target processor.

set(UPLOAD_SUPPORTS_DEBUG TRUE)

### Check if upload method can be enabled on this machine
find_package(OpenOCD)
set(UPLOAD_OPENOCD_FOUND ${OpenOCD_FOUND})

### Function to generate upload target
function(gen_upload_target TARGET_NAME BIN_FILE)

	# unlike other upload methods, OpenOCD uses the elf file
	add_custom_target(flash-${TARGET_NAME}
		COMMENT "Flashing ${TARGET_NAME} with OpenOCD..."
		COMMAND ${OpenOCD}
		${OPENOCD_CHIP_CONFIG_COMMANDS}
		-c "program $<TARGET_FILE:${TARGET_NAME}> reset exit")

	add_dependencies(flash-${TARGET_NAME} ${TARGET_NAME})
endfunction(gen_upload_target)

### Function to generate debug target
add_custom_target(gdbserver
	COMMENT "Starting OpenOCD GDB server"
	COMMAND
	${OpenOCD}
	${OPENOCD_CHIP_CONFIG_COMMANDS}
	-c "gdb_port ${GDB_PORT}"
	USES_TERMINAL)