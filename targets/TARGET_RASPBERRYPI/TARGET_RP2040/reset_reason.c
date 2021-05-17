#include "reset_reason_api.h"
#include "hardware/watchdog.h"

#ifdef DEVICE_RESET_REASON

#include "hardware/structs/resets.h"

reset_reason_t hal_reset_reason_get(void)
{
    if (watchdog_caused_reboot()) {
      return RESET_REASON_WATCHDOG;
    } else {
      return RESET_REASON_PIN_RESET;
    }

    return RESET_REASON_UNKNOWN;
}


uint32_t hal_reset_reason_get_raw(void)
{
    return resets_hw->reset;
}


void hal_reset_reason_clear(void)
{
    for (int i = 1; i < 25; i++) {
      hw_clear_bits(&resets_hw->reset, i);
    }
}

#endif // DEVICE_RESET_REASON
