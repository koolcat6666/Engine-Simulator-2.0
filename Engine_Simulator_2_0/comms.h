#ifndef __COMMS_H__
#define __COMMS_H__

#include <stdint.h>
#include <Arduino.h>

void serialSetup();
void commandParser();
uint16_t freeRam();
void toggle_invert_primary_cb();
void toggle_invert_secondary_cb();
void display_new_wheel();
void select_next_wheel_cb();

#endif
