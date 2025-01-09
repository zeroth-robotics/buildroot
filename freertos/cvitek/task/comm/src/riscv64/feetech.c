#include "uart.h"
#include "string.h"

#define SERVO_START_BYTE 0xFF
#define SERVO_BROADCAST_ID 0xFE

static uint8_t checksum(uint8_t *packet, size_t length) {
    uint16_t sum = 0;
    for (size_t i = 2; i < length - 1; i++) {
        sum += packet[i];
    }
    return ~(sum & 0xFF);
}

static int receive_packet(uint8_t *packet, size_t max_length) {
    size_t i = 0;
    while (i < max_length) {
        int c = uart_srv_getc();
        if (c < 0) {
            return -1; // Error or timeout
        }
        packet[i++] = (uint8_t)c;
        if (i >= 4 && i == packet[3] + 4) {
            break; // Received complete packet
        }
    }
    return i;
}

static int send_packet(uint8_t *packet, size_t length) {
    return uart_srv_put_array(packet, length);
}

int servo_write(uint8_t id, uint8_t address, uint8_t *data, uint8_t length) {
    uint8_t packet[256];  // Adjust size if needed
    uint8_t packet_length = length + 7;
    
    packet[0] = SERVO_START_BYTE;
    packet[1] = SERVO_START_BYTE;
    packet[2] = id;
    packet[3] = length + 3;  // data length + 3 (instruction + address + checksum)
    packet[4] = SERVO_CMD_WRITE;
    packet[5] = address;
    memcpy(&packet[6], data, length);
    packet[packet_length - 1] = calculate_checksum(packet, packet_length);
    
    if (send_packet(packet, packet_length) != packet_length) {
        return -1;
    }
    
    if (id != SERVO_BROADCAST_ID) {
        uint8_t response[6];
        int received = receive_packet(response, sizeof(response));
        
        if (received != 6 || response[2] != id || response[4] != 0) {
            return -1;
        }
    }
    
    return 0;
}

int servo_sync_write(uint8_t *data, size_t size) {
    uint8_t packet[MAX_SHMEM_DATA / 2];  // Adjust size if needed
    uint8_t packet_length = size + 4;
    
    packet[0] = SERVO_START_BYTE;
    packet[1] = SERVO_START_BYTE;
    packet[2] = SERVO_BROADCAST_ID;
    packet[3] = size + 2;  // data size + 2 (instruction + checksum)
    packet[4] = SERVO_CMD_SYNC_WRITE;
    memcpy(&packet[5], data, size);
    packet[packet_length - 1] = calculate_checksum(packet, packet_length);
    
    if (send_packet(packet, packet_length) != packet_length) {
        return -1;
    }
    
    return 0;
}

int servo_read(uint8_t id, uint8_t address, uint8_t length, uint8_t *data, int retry_count) {
    uint8_t packet[8] = {
        SERVO_START_BYTE, SERVO_START_BYTE,
        id,
        4,  // Length of the packet (always 4 for read instruction)
        SERVO_CMD_READ,
        address,
        length,
        0  // Checksum (to be calculated)
    };
    packet[7] = calculate_checksum(packet, 8);

    int result = -1;
    for (int retry = 0; retry < retry_count; retry++) {
        if (send_packet(packet, 8) != 8) {
            continue;  // Try again if send fails
        }

        // Receive response
        uint8_t response[256];  // Adjust size if needed
        int received = receive_packet(response, sizeof(response));

        if (received < 6 || response[2] != id) {
            g_read_servo_buffer.retry_count++;
            continue;  // Try again if response is invalid
        }

        // Copy received data to output buffer
        memcpy(data, &response[5], length);

        result = 0;  // Success
        break;
    }

    return result;
}


int servo_read_command(ServoCommand *cmd, uint8_t *response, int retry_count) {
    if (cmd->length > MAX_SERVO_COMMAND_DATA) {
        return -1; // Error: requested data length too long
    }

    // Prepare the response buffer
    response[0] = SERVO_START_BYTE;
    response[1] = SERVO_START_BYTE;
    response[2] = cmd->id;
    response[3] = cmd->length + 2; // data length + 2 (status + checksum)
    response[4] = 0; // Initialize status as 0 (success)

    // Read data from servo
    int result = servo_read(cmd->id, cmd->address, cmd->length, &response[5], retry_count);

    if (result != 0) {
        response[4] = 1; // Set status to 1 (error)
        response[5] = 0; // Clear data in case of error
        response[3] = 2; // Update length (only status + checksum)
    }

    // Calculate and add checksum
    response[response[3] + 3] = calculate_checksum(response, response[3] + 4);

    return response[3] + 4; // Return total length of response packet
}

int16_t servo_read_position_and_status(uint8_t id, ServoInfo *info, int retry_count) {
    uint8_t data[6];
    if (servo_read(id, SERVO_ADDR_CURRENT_POSITION, 6, data, retry_count) != 0) {
        return -1;  // Error
    }

    info->current_location = (int16_t)((data[1] << 8) | data[0]);
    info->current_speed = (int16_t)((data[3] << 8) | data[2]);
    info->current_load = (int16_t)((data[5] << 8) | data[4]);

    return 0;  // Success
}

int servo_read_info(uint8_t id, ServoInfo *info, int retry_count) {
    uint8_t data[30];
    if (servo_read(id, 0x28, 30, data, retry_count) != 0) {
        return -1;  // Error
    }

    info->torque_switch = data[0];
    info->acceleration = data[1];
    info->target_location = (int16_t)((data[3] << 8) | data[2]);
    info->running_time = (uint16_t)((data[5] << 8) | data[4]);
    info->running_speed = (uint16_t)((data[7] << 8) | data[6]);
    info->torque_limit = (uint16_t)((data[9] << 8) | data[8]);
    // Skip reserved bytes
    info->lock_mark = data[15];
    info->current_location = (int16_t)((data[17] << 8) | data[16]);
    info->current_speed = (int16_t)((data[19] << 8) | data[18]);
    info->current_load = (int16_t)((data[21] << 8) | data[20]);
    info->current_voltage = data[22];
    info->current_temperature = data[23];
    info->async_write_flag = data[24];
    info->servo_status = data[25];
    info->mobile_sign = data[26];
    // Skip reserved bytes
    info->current_current = (uint16_t)((data[29] << 8) | data[28]);

    return 0;  // Success
}

int servo_write_command(ServoCommand *cmd, int retry_count) {
    if (cmd->length > MAX_SERVO_COMMAND_DATA) {
        return -1; // Error: data too long
    }

    int result;
    for (int retry = 0; retry < retry_count; retry++) {
        result = servo_write(cmd->id, cmd->address, cmd->data, cmd->length);
        if (result == 0) {
            break;
        }
    }

    return result;
}