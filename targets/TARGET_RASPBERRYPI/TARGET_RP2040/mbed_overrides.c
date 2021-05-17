#include "cmsis.h"
#include "objects.h"
#include "platform/mbed_error.h"

int mbed_sdk_inited = 0;

void mbed_sdk_init()
{
	runtime_init();
	mbed_sdk_inited = 1;
}