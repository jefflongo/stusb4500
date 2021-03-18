# STUSB4500
STUSB4500 is a USB-C PD controller which supports a 5V fixed power profile, and two customizeable power profiles. This library allows for flashing the non-volatile memory of the STUSB4500 to change the default power profiles on boot as well as the capability to dynamically program the high priority power profile with the optimal power profile that the currently plugged in PD charger can supply. Optimal is defined as the highest wattage profile that satisfies a user-defined set of constraints. This code assumes a little endian architecture.

## Porting

This library can easily be ported to a custom platform. The only requirements are a function to get the current tick in ms (if using timeouts, recommended) and an i2c implementation. Add an implementation to the [platform-independent i2c wrapper library](https://github.com/jefflongo/libi2c). Please check the wrapper library README for details about providing the implementation. `i2c_master_init` should be called before using any of the stusb4500 library functions. Note that the library currently only supports the default slave address, and that an i2c speed should be at least 300kHz for dynamic power profiles. If there are additional requirements for porting the code to your own platform, please submit an issue so that the compatability can be further improved in the future.

To test if the i2c implementation is successful, the following code snippet should return true with the STUSB4500 connected to the i2c bus.
```c
uint8_t res;
i2c_master_read_u8(0x28U, 0x2FU, &res);
```

## Usage

### Dynamic Power Profiles
To take advantage of dynamic power profiles, include `stusb4500.h` and simply call `stusb_negotiate` to read the capabilities from the PD source and update/load the high priority power profile. `stusb_negotiate()` expects two parameters: a `stusb4500_config_t` configuration and a boolean `on_interrupt` which describes whether the function is being called on cable attachment interrupt or not.
- `stusb4500_config_t` has three adjustable parameters: minimum current, minimum voltage, maximum voltage. The optimal negotiated profile will satisfy these parameters. `stusb4500_config_t` also expects a function which returns the current tick in ms to handle timeout logic. If one is not provided, the timeout logic will not be used, which may cause the code to hang if something goes wrong.
-  If `on_interrupt` is `true`, `stusb4500_negotiate` will instantly start waiting to intercept the source capabilities message. If `on_interrupt` is `false`, `stusb4500_negotiate` will transmit a PD soft reset command to force a new transmission of source capabilities. If using `on_interrupt`, it may be desirable to call `stusb4500_negotiate` with `on_interrupt` set to `false` on boot to perform negotiation if a cable is already attached on boot.

### GPIO Control
STUSB4500 has a user controllable open-drain GPIO pin. The NVM can set whether the GPIO is controlled by the user or the STUSB4500. In the case of user control, the GPIO pin can be driven low or set to high-z by including `stusb4500.h` and calling `stusb4500_set_gpio_state`.

### NVM Flashing
The STUSB4500's non-volatile memory (NVM) determines what power profiles are configured at boot, along with some additional settings. Flashing the NVM is recommended if the circuit using the STUSB4500 cannot handle 20V, as the default NVM configuration contains a 20V profile. An NVM flash is required only once per chip. Currently, the following parameters can be adjusted via the `stusb4500_nvm_config_t` struct.

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
