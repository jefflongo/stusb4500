# STUSB4500
STUSB4500 is a USB-C PD controller which supports a 5V fixed power profile, and two customizeable power profiles. This library allows for flashing the non-volatile memory of the STUSB4500 to change the default power profiles on boot as well as the capability to program the high priority power profile with the optimal power profile that the currently plugged in PD charger can supply on the fly. Optimal is defined as the highest wattage profile that satisfies a user-defined set of constraints. From testing, an i2c clock >= 300 kHz is required to keep up with PD message speed. This code assumes a little endian architecture. 

This library can easily be ported to a custom platform. The only requirements are an i2c implementation (add an implementation to the platform-independent wrapper library [here](https://github.com/jefflongo/libi2c)), and a function to get the current tick for timing.

### Dynamic Power Profiles
To take advantage of dynamic power profiles, include `stusb4500.h`. `stusb4500_config_t` has three adjustable parameters: minimum current, minimum voltage, and maximum voltage. The optimal negotiated profile must satisfy these parameters. Simply call `stusb_negotiate()` to read the PD sources capabilities and update the high priority power profile. This is recommended to be done on charger attachment interrupt, but can also be done be done manually using a PD soft reset.

### NVM Flashing
Currently, the following parameters can be adjusted via the `stusb4500_config_t` struct.

| Parameter           | Description                             |
| ------------------- | --------------------------------------- |
| I_SNK_PDO1          | PDO1 current (mA)                       |
| V_SNK_PDO2          | PDO2 voltage (mV)                       |
| I_SNK_PDO2          | PDO2 current (mA)                       |
| V_SNK_PDO3          | PDO3 voltage (mV)                       |
| I_SNK_PDO3          | PDO3 current (mA)                       |
| I_SNK_PDO_FLEX      | Global PDO current if PDO = 0           |
| SNK_PDO_NUM         | Number of valid PDOs (1, 2, or 3)       |
| REQ_SRC_CURRENT     | Accept as much current as src provides  |
| POWER_ONLY_ABOVE_5V | Only output if negotiation above 5V     |
| GPIO_CFG            | Configures the behavior of the GPIO pin |

To program the NVM, include `stusb4500_nvm.h` and run `stusb4500_nvm_flash()` with your config. `stusb4500_nvm_flash()` returns true after writing and validating the flash.
