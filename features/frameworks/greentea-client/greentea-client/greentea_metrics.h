#include "platform/mbed_toolchain.h"

/** \addtogroup frameworks */
/** @{*/
#ifndef GREENTEA_METRICS_H
#define GREENTEA_METRICS_H

/**
 *  Setup platform specific metrics
 */
MBED_DEPRECATED_SINCE("mbed-os-6.13", "Greentea metrics API are deprecated")
void greentea_metrics_setup(void);

/**
 *  Report and cleanup platform specifc metrics
 */
MBED_DEPRECATED_SINCE("mbed-os-6.13", "Greentea metrics API are deprecated")
void greentea_metrics_report(void);

#endif

/** @}*/
