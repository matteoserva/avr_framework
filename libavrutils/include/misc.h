#ifndef _CM_MISC_H
#define _CM_MISC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t freeRam ();
void reboot_to_bootloader();
void watchdog_reboot();

#ifdef __cplusplus
}
#endif

#endif