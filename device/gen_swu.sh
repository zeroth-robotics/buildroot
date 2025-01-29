#!/bin/bash

# Check arguments
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <path-to-rootfs.ext4> <version>"
    exit 1
fi

# Arguments
ROOTFS_PATH="$1"
VERSION="$2"

# Ensure the rootfs file exists
if [ ! -f "$ROOTFS_PATH" ]; then
    echo "Error: RootFS file not found: $ROOTFS_PATH"
    exit 1
fi

# Determine the directory and filename
ROOTFS_DIR=$(dirname "$ROOTFS_PATH")
ROOTFS_FILENAME=$(basename "$ROOTFS_PATH")

# Define staging directory
SWU_DIR="/tmp/swu"

# Clean up and recreate staging directory
rm -rf "$SWU_DIR"
mkdir -p "$SWU_DIR"

# Copy and compress the rootfs
cp "$ROOTFS_PATH" "$SWU_DIR/rootfs.ext4"
gzip -f "$SWU_DIR/rootfs.ext4"

# Generate SHA256 checksum
SHA256SUM=$(sha256sum "$SWU_DIR/rootfs.ext4.gz" | awk '{print $1}')

# Create sw-description file
SW_DESCRIPTION="$SWU_DIR/sw-description"
cat > "$SW_DESCRIPTION" <<EOF
software = {
    version = "$VERSION";
    description = "ZBot Update Package";
    hardware_compatibility = ["1.0", "1.1"];
    images = (
        {
            filename = "rootfs.ext4.gz";
            device = "/dev/mmcblk0p4";
            type = "raw";
            sha256 = "$SHA256SUM";
            compressed = "zlib";
        }
    );
}
EOF

# Create the SWU package
cd "$SWU_DIR" || exit 1
export FILES="sw-description rootfs.ext4.gz"
for i in $FILES; do echo "$i"; done | cpio -ov -H crc > "zbot.swu"

# Move the SWU package to the rootfs directory
mv "$SWU_DIR/zbot.swu" "$ROOTFS_DIR/zbot.swu"

# Cleanup temporary directory
rm -rf "$SWU_DIR"

echo "SWU package created: $ROOTFS_DIR/zbot.swu"
