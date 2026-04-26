#ifndef __MAIN_H
#define __MAIN_H

/* Firmware version.
 * Increment FIRMWARE_VERSION_MAJOR for breaking or significant changes.
 * Increment FIRMWARE_VERSION_MINOR for small, backward-compatible changes;
 * reset to 0 whenever the major version is incremented. */
#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 0

@far @interrupt void tim1UpdateInterrupt(void);

#endif
