#pragma once

#include <stdbool.h>
#include <stdint.h>

bool nvm_flash(void);
bool nvm_read(uint8_t* sectors_out);
bool nvm_verify(void);
