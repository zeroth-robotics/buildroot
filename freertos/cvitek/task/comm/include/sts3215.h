#ifndef SERVO_DRIVER_H
#define SERVO_DRIVER_H

#include <stdint.h>
#include <stddef.h>

// Servo commands
#define SERVO_CMD_PING 0x01
#define SERVO_CMD_READ 0x02
#define SERVO_CMD_WRITE 0x03
#define SERVO_CMD_REG_WRITE 0x04
#define SERVO_CMD_ACTION 0x05
#define SERVO_CMD_SYNC_WRITE 0x83
#define SERVO_CMD_RESET 0x06
#define SERVO_ADDR_TORQUE_SWITCH 0x28


// Memory addresses
#define SERVO_ADDR_TARGET_POSITION 0x2A
#define SERVO_ADDR_CURRENT_POSITION 0x38
#define SERVO_ADDR_CURRENT_LOAD 0x3C
#define SERVO_ADDR_CURRENT_VOLTAGE 0x3E
#define SERVO_ADDR_CURRENT_CURRENT 0x45
#define TORQUE_OFF 0
#define TORQUE_ON 1

#define MAX_SERVO_COMMAND_DATA 256
#define MAX_SERVOS 16

typedef struct {
    uint8_t torque_switch;         // 0x28 (1 byte)
    uint8_t acceleration;          // 0x29 (1 byte)
    int16_t target_location;       // 0x2A (2 bytes)
    uint16_t running_time;         // 0x2C (2 bytes)
    uint16_t running_speed;        // 0x2E (2 bytes)
    uint16_t torque_limit;         // 0x30 (2 bytes)
    uint8_t reserved1[6];          // 0x32-0x37 (6 bytes, reserved)
    uint8_t lock_mark;             // 0x37 (1 byte)
    int16_t current_location;      // 0x38 (2 bytes)
    int16_t current_speed;         // 0x3A (2 bytes)
    int16_t current_load;          // 0x3C (2 bytes)
    uint8_t current_voltage;       // 0x3E (1 byte)
    uint8_t current_temperature;   // 0x3F (1 byte)
    uint8_t async_write_flag;      // 0x40 (1 byte)
    uint8_t servo_status;          // 0x41 (1 byte)
    uint8_t mobile_sign;           // 0x42 (1 byte)
    uint8_t reserved2[2];          // 0x43-0x44 (2 bytes, reserved)
    uint16_t current_current;      // 0x45 (2 bytes)
} ServoInfo;

typedef struct {
    uint8_t id;
    uint8_t address;
    uint8_t length;
    uint8_t data[MAX_SERVO_COMMAND_DATA];
} ServoCommand;

typedef struct {
    uint8_t only_write_positions;
    uint8_t ids[MAX_SERVOS];
    int16_t positions[MAX_SERVOS];
    uint16_t times[MAX_SERVOS];
    uint16_t speeds[MAX_SERVOS];
} ServoMultipleWriteCommand;

// Function prototypes
void servo_init(void);
int servo_ping(uint8_t id);
int servo_read(uint8_t id, uint8_t address, uint8_t length, uint8_t *data, int retry_count);
int servo_read_command(ServoCommand *cmd, uint8_t *data, int retry_count);
int servo_write(uint8_t id, uint8_t address, uint8_t *data, uint8_t length);
int servo_write_command(ServoCommand *cmd);
int servo_reg_write(uint8_t id, uint8_t address, uint8_t *data, uint8_t length);
int servo_action(void);
int servo_sync_write(uint8_t *data, size_t size);
int servo_reset(uint8_t id);

// New function prototypes
int servo_move(uint8_t id, int16_t position, uint16_t time, uint16_t speed);
int servo_move_multiple(uint8_t *ids, int16_t *positions, uint8_t count);
int servo_move_multiple_sync(uint8_t *ids, int16_t *positions, uint16_t *times, uint16_t *speeds, uint8_t count, uint8_t only_write_positions);
int16_t servo_read_position(uint8_t id);
uint16_t servo_read_current(uint8_t id);
int16_t servo_read_load(uint8_t id);
uint8_t servo_read_voltage(uint8_t id);
int servo_torque_on(uint8_t id);
int servo_torque_off(uint8_t id);
int servo_set_torque(uint8_t id, uint8_t torque_state);
int16_t read_16bit_value(uint8_t id, uint8_t address);

#endif // SERVO_DRIVER_H
