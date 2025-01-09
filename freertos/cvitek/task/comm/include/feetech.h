#ifndef FEETECH_H
#define FEETECH_H

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

#define MAX_SERVO_COMMAND_DATA 40
#define MAX_SHMEM_DATA 2048
#define MAX_SERVOS 32

typedef struct {
    uint8_t id;
    uint32_t last_read_ms;
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
    uint32_t data_length;
    uint8_t data[MAX_SHMEM_DATA];
} BroadcastCommand;

typedef struct {
    uint32_t retry_count;
    uint32_t read_count;
    uint32_t loop_count;
    uint32_t fault_count;
    uint32_t last_read_ms;
    ServoInfo servos[MAX_SERVOS];
} ServoInfoBuffer;

typedef struct {
    uint32_t len;
    uint8_t servo_id[MAX_SERVOS];
} ActiveServoList;

int servo_write(uint8_t id, uint8_t address, uint8_t *data, uint8_t length);
int servo_sync_write(uint8_t *data, size_t size);
int servo_read(uint8_t id, uint8_t address, uint8_t length, uint8_t *data, int retry_count);
int servo_read_command(ServoCommand *cmd, uint8_t *data, int retry_count);
int16_t servo_read_position_and_status(uint8_t id, ServoInfo *info, int retry_count);
int servo_read_info(uint8_t id, ServoInfo *info, int retry_count);

#endif
