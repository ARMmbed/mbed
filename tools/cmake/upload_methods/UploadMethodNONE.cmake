# Copyright (c) 2020 ARM Limited. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

### No-op upload method.
# This method needs no parameters.

set(UPLOAD_NONE_FOUND TRUE) # this has no dependencies
set(UPLOAD_SUPPORTS_DEBUG FALSE)

### Function to generate upload target

function(gen_upload_target TARGET_NAME BIN_FILE)

	function(gen_upload_target TARGET_NAME BIN_FILE)
		add_custom_target(flash-${TARGET_NAME}
			COMMAND ${CMAKE_COMMAND}
			-E echo "Cannot flash, no UPLOAD_METHOD configured!")

		add_dependencies(flash-${TARGET_NAME} ${TARGET_NAME})

	endfunction(gen_upload_target)

endfunction(gen_upload_target)