#include "stusb4500.h"

#include "i2c.h"

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
#define PE_FSM 0x29
#define WHO_AM_I 0x2F
#define RX_BYTE_CNT 0x30
#define RX_HEADER 0x31
#define RX_DATA_OBJ 0x33
#define TX_HEADER 0x51
#define DPM_SNK_PDO1 0x85

// STUSB4500 masks/constants
#define STUSB4500_ID 0x25
#define STUSB4500B_ID 0x21
#define SW_RESET_ON 0x01
#define SW_RESET_OFF 0x00
#define ATTACH 0x01
#define PRT_MESSAGE_RECEIVED 0x04
#define SRC_CAPABILITIES_MSG 0x01
#define PE_SNK_READY 0x18

// Maximum number of source power profiles
#define MAX_SRC_PDOS 10

// PD protocol commands, see USB PD spec Table 6-3
#define PD_CMD 0x26
#define PD_SOFT_RESET 0x000D

// See USB PD spec Table 6-1
#define MESSAGE_HEADER_POS 0
#ifdef USBPD_REV30_SUPPORT
#define MESSAGE_HEADER_MSK (0x1F << MESSAGE_HEADER_POS)
#else // USBPD_REV30_SUPPORT
#define MESSAGE_HEADER_MSK (0x0F << MESSAGE_HEADER_POS)
#endif // USBPD_REV30_SUPPORT
#define HEADER_NUM_DATA_OBJECTS_POS 12
#define HEADER_NUM_DATA_OBJECTS_MSK (0x07 << HEADER_NUM_DATA_OBJECTS_POS)
#define HEADER_MESSAGE_TYPE(header) (((header)&MESSAGE_HEADER_MSK) >> MESSAGE_HEADER_POS)
#define HEADER_NUM_DATA_OBJECTS(header)                                                            \
    (((header)&HEADER_NUM_DATA_OBJECTS_MSK) >> HEADER_NUM_DATA_OBJECTS_POS)

// See USB PD spec Section 7.1.3 and STUSB4500 Section 5.2 Table 16
#define PDO_TYPE_POS 30
#define PDO_TYPE_MSK (0x03 << PDO_TYPE_POS)
#define PDO_TYPE(pdo) (((pdo)&PDO_TYPE_MSK) >> PDO_TYPE_POS)
#define PDO_TYPE_FIXED 0x00

#define PDO_CURRENT_POS 0
#define PDO_CURRENT_MSK (0x03FF << PDO_CURRENT_POS)
#define PDO_CURRENT_RESOLUTION 10
#define FROM_PDO_CURRENT(pdo)                                                                      \
    ((((pdo)&PDO_CURRENT_MSK) >> PDO_CURRENT_POS) * PDO_CURRENT_RESOLUTION)
#define TO_PDO_CURRENT(ma) ((((ma) / PDO_CURRENT_RESOLUTION) << PDO_CURRENT_POS) & PDO_CURRENT_MSK)

#define PDO_VOLTAGE_POS 10
#define PDO_VOLTAGE_MSK (0x03FF << PDO_VOLTAGE_POS)
#define PDO_VOLTAGE_RESOLUTION 50
#define FROM_PDO_VOLTAGE(pdo)                                                                      \
    ((((pdo)&PDO_VOLTAGE_MSK) >> PDO_VOLTAGE_POS) * PDO_VOLTAGE_RESOLUTION)
#define TO_PDO_VOLTAGE(mv) ((((mv) / PDO_VOLTAGE_RESOLUTION) << PDO_VOLTAGE_POS) & PDO_VOLTAGE_MSK)

#define TIMEOUT_MS 500

typedef uint32_t stusb4500_power_t;
typedef uint32_t stusb4500_pdo_t;
typedef uint8_t stusb4500_pd_state_t;

// PD_SOFT_RESET seems to be the only message the STUSB4500 supports
static bool send_pd_message(const uint16_t msg) {
    return (
      i2c_master_write_u16(STUSB_ADDR, TX_HEADER, msg) &&
      i2c_master_write_u8(STUSB_ADDR, CMD_CTRL, PD_CMD));
}

static bool is_present(void) {
    uint8_t res;
    if (!i2c_master_read_u8(STUSB_ADDR, WHO_AM_I, &res)) return false;

    return (res == STUSB4500_ID || res == STUSB4500B_ID);
}

static bool wait_until_ready_with_timeout(stusb4500_config_t* config) {
    stusb4500_pd_state_t pd_state;
    uint32_t start = 0;

    if (config->get_ms) {
        start = config->get_ms();
    }

    do {
        if (config->get_ms && (config->get_ms() - start > TIMEOUT_MS)) return false;
        if (!i2c_master_read_u8(STUSB_ADDR, PE_FSM, &pd_state)) return false;
    } while (pd_state != PE_SNK_READY);

    return true;
}

static bool
  write_pdo(stusb4500_current_t current_ma, stusb4500_voltage_t voltage_mv, uint8_t pdo_num) {
    if (pdo_num < 1 || pdo_num > 3) return false;

    // Format the sink PDO
    stusb4500_pdo_t pdo = TO_PDO_CURRENT(current_ma) | TO_PDO_VOLTAGE(voltage_mv);

    // Write the sink PDO
    return i2c_master_write_u32(
      STUSB_ADDR, DPM_SNK_PDO1 + sizeof(stusb4500_pdo_t) * (pdo_num - 1), pdo);
}

static bool
  load_optimal_pdo(stusb4500_config_t* config, stusb4500_pdo_t* src_pdos, uint8_t num_pdos) {
    bool found = false;

    stusb4500_current_t opt_pdo_current = config->min_current_ma;
    stusb4500_voltage_t opt_pdo_voltage = config->min_voltage_mv;
    stusb4500_power_t opt_pdo_power = opt_pdo_voltage * opt_pdo_current / 1000;

    // Search for the optimal PDO, if any
    for (int i = 0; i < num_pdos; i++) {
        stusb4500_pdo_t pdo = src_pdos[i];

        // Extract PDO parameters
        stusb4500_current_t pdo_current = FROM_PDO_CURRENT(pdo);
        stusb4500_voltage_t pdo_voltage = FROM_PDO_VOLTAGE(pdo);
        stusb4500_power_t pdo_power = pdo_current * pdo_voltage / 1000;

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
          PDO_TYPE(pdo) != PDO_TYPE_FIXED || pdo_current < config->min_current_ma ||
          pdo_voltage < config->min_voltage_mv || pdo_voltage > config->max_voltage_mv)
            continue;
        if (pdo_power > opt_pdo_power) {
            opt_pdo_current = pdo_current;
            opt_pdo_voltage = pdo_voltage;
            opt_pdo_power = pdo_power;
            found = true;
        }
    }

#ifdef STUSB4500_ENABLE_PRINTF
    printf(
      "\r\nSelecting optimal PDO based on user parameters: %d.%03dV - %d.%03dV, >= "
      "%d.%03dA\r\n",
      (int)(config->min_voltage_mv / 1000),
      (int)(config->min_voltage_mv % 1000),
      (int)(config->max_voltage_mv / 1000),
      (int)(config->max_voltage_mv % 1000),
      (int)(config->min_current_ma / 1000),
      (int)(config->min_current_ma % 1000));
    if (found) {
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
        return false;
    }
#else
    if (!found) return false;
#endif // STUSB4500_ENABLE_PRINTF

    // Push the new PDO
    if (!write_pdo(opt_pdo_current, opt_pdo_voltage, 3)) return false;

    return true;
}

bool stusb4500_negotiate(stusb4500_config_t* config, bool on_interrupt) {
    uint8_t buffer[MAX_SRC_PDOS * sizeof(stusb4500_pdo_t)];
    uint16_t header;
    uint32_t start = 0;

    // Sanity check to see if STUSB4500 is there
    if (!is_present()) return false;

    // Check that cable is attached
    if (!i2c_master_read_u8(STUSB_ADDR, PORT_STATUS, buffer) || !(buffer[0] & ATTACH)) return false;

    // Force transmission of source capabilities if not responding to an ATTACH interrupt
    if (!on_interrupt) {
        if (!wait_until_ready_with_timeout(config)) return false;
        if (!send_pd_message(PD_SOFT_RESET)) return false;
    }

    if (config->get_ms) {
        start = config->get_ms();
    }

    while (1) {
        // Check for timeout
        if (config->get_ms && (config->get_ms() - start > TIMEOUT_MS)) return false;

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
        if (buffer[0] != HEADER_NUM_DATA_OBJECTS(header) * sizeof(stusb4500_pdo_t)) return false;

        break;
    }

    // Read source capabilities
    // WARNING: This must happen very soon after the previous code block is executed. The source
    // will send an accept message which partially overwrites the source capabilities message.
    // Use i2c clock >= 300 kHz
    if (!i2c_master_read(
          STUSB_ADDR,
          RX_DATA_OBJ,
          buffer,
          HEADER_NUM_DATA_OBJECTS(header) * sizeof(stusb4500_pdo_t)))
        return false;

    // Wait for idle state before loading new PDO
    if (!wait_until_ready_with_timeout(config)) return false;

    // Find and load the optimal PDO, if any
    if (!load_optimal_pdo(config, (stusb4500_pdo_t*)buffer, HEADER_NUM_DATA_OBJECTS(header)))
        return false;

    // Force a renegotiation
    return send_pd_message(PD_SOFT_RESET);
}
