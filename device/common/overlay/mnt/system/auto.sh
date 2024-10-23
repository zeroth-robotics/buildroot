#!/bin/sh

max_attempts=15
log_file="/var/log/auto.sh.log"

echo "start auto.sh" > "$log_file"

# Function to wait for valid MAC addresses
wait_for_mac_addresses() {
    attempt=0

    while [ $attempt -lt $max_attempts ]; do
        mac1=$(cat /tmp/mac1)
        mac2=$(cat /tmp/mac2)

        # Check if both MAC addresses are non-empty and in the correct format
        if [ -n "$mac1" ] && [ -n "$mac2" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') Valid MAC addresses obtained: MAC1=$mac1, MAC2=$mac2" >> "$log_file"
            return 0
        fi

        echo "$(date +'%Y-%m-%d %H:%M:%S') Waiting for valid MAC addresses... (Attempt $((attempt+1))/$max_attempts)" >> "$log_file"
        sleep 1
        attempt=$((attempt + 1))
    done

    echo "$(date +'%Y-%m-%d %H:%M:%S') Failed to obtain valid MAC addresses after $max_attempts attempts" >> "$log_file"
    return 1
}

# Function to wait for interface and set MAC address
wait_and_set_mac() {
    interface=$1
    mac_address=$2
    attempt=0

    while [ $attempt -lt $max_attempts ]; do
        if ip link show "$interface" > /dev/null 2>&1; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') $interface interface exists, setting MAC address..." >> "$log_file"
            ip link set "$interface" down
            ip link set "$interface" address "$mac_address"
            ip link set "$interface" up
            echo "$(date +'%Y-%m-%d %H:%M:%S') MAC address for $interface set to $mac_address" >> "$log_file"
            return 0
        else
            echo "$(date +'%Y-%m-%d %H:%M:%S') $interface interface not found, waiting... (Attempt $((attempt+1))/$max_attempts)" >> "$log_file"
            sleep 1
            attempt=$((attempt + 1))
        fi
    done

    echo "$(date +'%Y-%m-%d %H:%M:%S') Interface $interface not found after $max_attempts attempts" >> "$log_file"
    return 1
}

# Wait for valid MAC addresses
if ! wait_for_mac_addresses; then
    echo "$(date +'%Y-%m-%d %H:%M:%S') Failed to obtain valid MAC addresses, exiting" >> "$log_file"
    exit 1
fi

# Wait for eth0 and set its MAC address
wait_and_set_mac "eth0" "$mac1"

# Wait for wlan0 and set its MAC address
wait_and_set_mac "wlan0" "$mac2"

# Start wpa_supplicant for wlan0 if it exists
if ip link show wlan0 > /dev/null 2>&1; then
    echo "$(date +'%Y-%m-%d %H:%M:%S') Starting wpa_supplicant for wlan0..." >> "$log_file"
    wpa_supplicant -B -i wlan0 -c /boot/wpa_supplicant.conf >> "$log_file" 2>&1
else
    echo "$(date +'%Y-%m-%d %H:%M:%S') wlan0 interface not available, skipping wpa_supplicant" >> "$log_file"
fi

# Start sts_server
echo "$(date +'%Y-%m-%d %H:%M:%S') Starting sts_server..." >> "$log_file"
nohup /usr/local/bin/sts_server > /var/log/sts_server.log 2>&1 &