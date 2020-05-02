#include "stusb4500.h"

#include "i2c.h"

#include <assert.h>
#include <stdint.h>

#ifdef STUSB4500_ENABLE_PRINTF
#include <stdio.h>
#endif // STUSB4500_ENABLE_PRINTF

// STUSB4500 i2c address
#define STUSB_ADDR 0x28

// STUSB4500 registers
#define PORT_STATUS 0x0E
#define PRT_STATUS 0x16
#define CMD_CTRL 0x1A
#define RESET_CTRL 0x23
#define WHO_AM_I 0x2F
#define RX_BYTE_CNT 0x30
#define RX_HEADER 0x31
#define RX_DATA_OBJ 0x33
#define TX_HEADER 0x51
#define DPM_SNK_PDO1 0x85

// STUSB4500 masks
#define STUSB4500_ID 0x25
#define STUSB4500B_ID 0x21
#define SW_RESET_ON 0x01
#define SW_RESET_OFF 0x00
#define ATTACH 0x01
#define PRT_MESSAGE_RECEIVED 0x04
#define SRC_CAPABILITIES_MSG 0x01

// Maximum number of source power profiles
#define MAX_SRC_PDOS 10

// PD protocol commands, see USB PD spec Table 6-3
#define PD_CMD 0x26
#define PD_GET_SRC_CAP 0x07
#define PD_SOFT_RESET 0x0D

// See USB PD spec Table 6-1
#ifdef USBPD_REV30_SUPPORT
#define HEADER_MESSAGE_TYPE(header) (header & 0x1F)
#else // USBPD_REV30_SUPPORT
#define HEADER_MESSAGE_TYPE(header) (header & 0x0F)
#endif // USBPD_REV30_SUPPORT
#define HEADER_NUM_DATA_OBJECTS(header) ((header >> 12) & 0x07)

// See USB PD spec Table 6-9
#define PDO_SIZE 4

// See USB PD spec Section 7.1.3 and STUSB4500 Section 5.2 Table 16
#define PDO_TYPE(pdo) ((pdo >> 30) & 0x03)
#define PDO_TYPE_FIXED 0x00
#define FROM_PDO_CURRENT(pdo) ((pdo & 0x03FF) * 10)
#define FROM_PDO_VOLTAGE(pdo) (((pdo >> 10) & 0x03FF) * 50)
#define TO_PDO_CURRENT(mA) ((mA / 10) & 0x03FF)
#define TO_PDO_VOLTAGE(mV) ((uint32_t)((mV / 50) & 0x03FF) << 10)

#define PLUG_TIMEOUT 3000
#define RETRIEVE_TIMEOUT 500

// TODO: This doesn't work, STUSB4500 likely doesn't support this, need to verify
__attribute__((unused)) static bool send_pd_message(const uint8_t msg) {
    bool ok = true;

    if (ok) ok = i2c_master_write_u8(STUSB_ADDR, TX_HEADER, msg);
    if (ok) ok = i2c_master_write_u8(STUSB_ADDR, CMD_CTRL, PD_CMD);

    return ok;
}

static bool is_present(void) {
    uint32_t time = STUSB4500_GET_MS();
    uint8_t res;
    do {
        if (!i2c_master_read_u8(STUSB_ADDR, WHO_AM_I, &res)) return false;
        if (STUSB4500_GET_MS() - time > RETRIEVE_TIMEOUT) return false;
    } while (res != STUSB4500_ID && res != STUSB4500B_ID);

    return true;
}

static bool reset(void) {
    bool ok = true;

    // Enable software reset
    if (ok) ok = i2c_master_write_u8(STUSB_ADDR, RESET_CTRL, SW_RESET_ON);

    // Wait for stusb to respond
    if (ok) ok = is_present();

    // TODO: Necessary? Wait for source to be ready
    if (ok) STUSB4500_DELAY_MS(27);

    // Disable software reset
    if (ok) ok = i2c_master_write_u8(STUSB_ADDR, RESET_CTRL, SW_RESET_OFF);

    return ok;
}

static bool write_pdo(uint16_t current_mA, uint16_t voltage_mV, uint8_t pdo_num) {
    if (pdo_num < 1 || pdo_num > 3) return false;

    // Format the sink PDO
    uint32_t pdo = TO_PDO_CURRENT(current_mA) | TO_PDO_VOLTAGE(voltage_mV);

    // Write the sink PDO
    return i2c_master_write_u32(STUSB_ADDR, DPM_SNK_PDO1 + PDO_SIZE * (pdo_num - 1), pdo);
}

static bool load_optimal_pdo(uint32_t* src_pdos, uint8_t num_pdos) {
    bool ok = false;

    uint16_t opt_pdo_current = 0;
    uint16_t opt_pdo_voltage = 0;
    uint32_t opt_pdo_power = 0;

    // Search for the optimal PDO, if any
    for (int i = 0; i < num_pdos; i++) {
        uint32_t pdo = src_pdos[i];

        // Extract PDO parameters
        uint16_t pdo_current = FROM_PDO_CURRENT(pdo);
        uint16_t pdo_voltage = FROM_PDO_VOLTAGE(pdo);
        uint32_t pdo_power = pdo_current * pdo_voltage / 1000;

#ifdef STUSB4500_ENABLE_PRINTF
        printf(
          "Detected Source PDO: %2d.%03dV, %d.%03dA, %3d.%03dW\r\n",
          (int)(pdo_voltage / 1000),
          (int)(pdo_voltage % 1000),
          (int)(pdo_current / 1000),
          (int)(pdo_current % 1000),
          (int)(pdo_power / 1000),
          (int)(pdo_power % 1000));
#endif // STUSB4500_ENABLE_PRINTF

        if (
          PDO_TYPE(pdo) != PDO_TYPE_FIXED || pdo_current < PDO_CURRENT_MIN ||
          pdo_voltage < PDO_VOLTAGE_MIN || pdo_voltage > PDO_VOLTAGE_MAX)
            continue;
        if (pdo_power > opt_pdo_power) {
            opt_pdo_current = pdo_current;
            opt_pdo_voltage = pdo_voltage;
            opt_pdo_power = pdo_power;
            ok = true;
        }
    }

#ifdef STUSB4500_ENABLE_PRINTF
    printf(
      "\r\nSelecting optimal PDO based on user parameters: %d.%03dV - %d.%03dV, >= "
      "%d.%03dA\r\n",
      (int)(PDO_VOLTAGE_MIN / 1000),
      (int)(PDO_VOLTAGE_MIN % 1000),
      (int)(PDO_VOLTAGE_MAX / 1000),
      (int)(PDO_VOLTAGE_MAX % 1000),
      (int)(PDO_CURRENT_MIN / 1000),
      (int)(PDO_CURRENT_MIN % 1000));
    if (ok) {
        printf(
          "Selected PDO: %d.%03dV, %d.%03dA, %d.%03dW\r\n\r\n",
          (int)(opt_pdo_voltage / 1000),
          (int)(opt_pdo_voltage % 1000),
          (int)(opt_pdo_current / 1000),
          (int)(opt_pdo_current % 1000),
          (int)(opt_pdo_power / 1000),
          (int)(opt_pdo_power % 1000));
    } else {
        printf("No suitable PDO found\r\n\r\n");
    }
#endif // STUSB4500_ENABLE_PRINTF

    // Push the new PDO
    if (ok) ok = write_pdo(opt_pdo_current, opt_pdo_voltage, 3);

    return ok;
}

bool stusb_negotiate(bool on_interrupt) {
    // Sanity check to see if STUSB is there
    if (!is_present()) return false;

    // Force transmission of source capabilities if not responding to an ATTACH interrupt
    if (!on_interrupt && !reset()) return false;

    uint8_t buffer[MAX_SRC_PDOS * PDO_SIZE];
    uint16_t header;
    uint32_t time = STUSB4500_GET_MS();

    // Provide a buffer for cable attachment
    do {
        if (!i2c_master_read_u8(STUSB_ADDR, PORT_STATUS, buffer)) return false;
        if (STUSB4500_GET_MS() - time > PLUG_TIMEOUT) return false;
    } while (!(buffer[0] & ATTACH));

    time = STUSB4500_GET_MS();

    while (1) {
        // Check for timeout
        if (STUSB4500_GET_MS() - time > RETRIEVE_TIMEOUT) return false;

        // Read the port status to look for a source capabilities message
        if (!i2c_master_read_u8(STUSB_ADDR, PRT_STATUS, buffer)) return false;

        // Message has not arrived yet
        if (!(buffer[0] & PRT_MESSAGE_RECEIVED)) continue;

        // Read message header
        if (!i2c_master_read_u16(STUSB_ADDR, RX_HEADER, &header)) return false;

        // Not a data/source capabilities message, continue waiting
        if (!HEADER_NUM_DATA_OBJECTS(header) || HEADER_MESSAGE_TYPE(header) != SRC_CAPABILITIES_MSG)
            continue;

        // Read number of received bytes
        if (!i2c_master_read_u8(STUSB_ADDR, RX_BYTE_CNT, buffer)) return false;

        // Check for missing data
        if (buffer[0] != HEADER_NUM_DATA_OBJECTS(header) * PDO_SIZE) return false;

        break;
    }

    // Read source capabilities
    // WARNING: This must happen very soon after the previous code block is
    // executed. The source will send an accept message which partially
    // overwrites the source capabilities message. Use i2c clock >= 300 kHz
    if (!i2c_master_read(
          STUSB_ADDR, RX_DATA_OBJ, buffer, HEADER_NUM_DATA_OBJECTS(header) * PDO_SIZE))
        return false;

    // Find and load the optimal PDO, if any
    if (!load_optimal_pdo((uint32_t*)buffer, HEADER_NUM_DATA_OBJECTS(header))) return false;

    // Force a renegotiation
    // NOTE: vbus will be momentarily lost
    return reset();
}