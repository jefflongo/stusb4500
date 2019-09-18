# STUSB4500
The STUSB4500 supports a 5V fixed power profile, and two customizeable power profiles. This library allows for flashing the non-volatile memory of the STUSB4500 to change the default power profiles on boot as well as the capability to program the high priority power profile with the optimal power profile that the currently plugged in PD charger can supply on the fly. Optimal is defined as the highest wattage profile that satisfies a user-defined set of constraints. A PIC i2c driver is included in this code base. Note that `config.h` is missing from this code base, this simply defines `_XTAL_FREQ` for the PIC environment. This code is portable with minor modification, namely an i2c and delay implementation. This code assumes a little endian architecture.

### NVM Flashing
`stusb4500_nvm_config.h` contains the configuration to be flashed to the STUSB4500 NVM. The following parameters can be adjusted:

| Parameter           | Description                            | Default Value |
| --------------------| -------------------------------------- | ------------- |
| I_SNK_PDO1          | PDO1 current (mA)                      | 1.5A          |
| V_SNK_PDO2          | PDO2 voltage (mV)                      | 9V            |
| I_SNK_PDO2          | PDO2 current (mA)                      | 3A            |
| V_SNK_PDO3          | PDO3 voltage (mV)                      | 15V           |
| I_SNK_PDO3          | PDO3 current (mA)                      | 2A            |
| I_SNK_PDO_FLEX      | Global PDO current if PDO = 0          | 2A            |
| SNK_PDO_NUM         | Number of valid PDOs (1, 2, or 3)      | 3             |
| REQ_SRC_CURRENT     | Accept as much current as src provides | Yes           |
| POWER_ONLY_ABOVE_5V | Only output if negotiation above 5V    | No            |

To program the NVM, include `nvm_flash.h` and run `nvm_flash()`. It is recommended to run `nvm_verify()` after to check if the flash was successful (returns 0 on success). 

### Dynamic Power Profiles
To take advantage of dynamic power profiles, include `stusb4500.h`. `stusb4500.h` has three adjustable parameters that the user can adjust: minimum current, minimum voltage, and maximum voltage. The optimal negotiated profile must satisfy these parameters. Simply call `stusb_negotiate()` to read the PD sources capabilities and update the high priority power profile. This is recommended to be done on charger attachment interrupt, but can also be done occasionally on a timer if this capability is not available.
