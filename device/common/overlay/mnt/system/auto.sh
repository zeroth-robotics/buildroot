#!/bin/sh

interface="wlan0"
max_attempts=100
attempt=0
log_file="/var/log/auto.sh.log"

# Get MAC addresses from efuse_read_serial
mac_addresses=$(efuse_read_serial | grep "MAC" | awk '{print $2}')
mac1=$(echo "$mac_addresses" | sed -n '2p')
mac2=$(echo "$mac_addresses" | sed -n '3p')

# Set static MAC addresses
ip link set eth0 down
ip link set wlan0 down
ip link set eth0 address "$mac1"
ip link set wlan0 address "$mac2"
ip link set eth0 up
ip link set wlan0 up

# Continuously attempt to detect if the interface exists, up to $max_attempts times
echo "start auto.sh" > "$log_file"

nohup /usr/local/bin/sts_server > /var/log/sts_server.log 2>&1 &

while [ $attempt -lt $max_attempts ]; do
    # Check if the wlan0 interface exists
    ip link show "$interface" > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "$(date +'%Y-%m-%d %H:%M:%S') $interface interface exists, starting wpa_supplicant..." >> "$log_file"
        wpa_supplicant -B -i "$interface" -c /boot/wpa_supplicant.conf >> "$log_file"
        break  # Exit the loop if the interface is found
    else
        echo "$(date +'%Y-%m-%d %H:%M:%S') $interface interface not found, waiting..." >> "$log_file"
        sleep 1  # Wait for 1 second before checking again
        attempt=$((attempt + 1))  # Increment the attempt counter
    fi
done

# If the maximum number of attempts is reached and the interface still not found, output an error message
if [ $attempt -eq $max_attempts ]; then
    echo "$(date +'%Y-%m-%d %H:%M:%S') Interface $interface not found after $max_attempts attempts" >> "$log_file"
fi
