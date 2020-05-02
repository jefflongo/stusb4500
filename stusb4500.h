#pragma once

#include <stdbool.h>

// Define these platform-specific functions
#include "sys/sys.h"
#define STUSB4500_GET_MS() SYS_GET_MS()
#define STUSB4500_DELAY_MS(ms) SYS_DELAY_MS(ms)

// Enable logging via printf
#define STUSB4500_ENABLE_PRINTF

// User adjustable parameters
#define PDO_CURRENT_MIN 1500  // mA, 25mA increments
#define PDO_VOLTAGE_MIN 5000  // mV, 50mV increments
#define PDO_VOLTAGE_MAX 12000 // mV, 50mV increments

bool stusb_negotiate(bool on_interrupt);
