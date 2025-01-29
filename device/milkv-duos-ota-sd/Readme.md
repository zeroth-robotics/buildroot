------------------------------------
Build ZBot OTA System
------------------------------------
```
source device/milkv-duos-ota-sd/boardconfig.sh 
source build/milkvsetup-ota.sh 
defconfig cv1813h_milkv_duos_ota_sd
clean_all

# run mkcvipary manually **FIXME** (automate this)
python build/tools/common/image_tool/mkcvipart.py  build/boards/cv181x/cv1813h_milkv_duos_ota_sd/partition/partition_sd.xml u-boot-2021.10/include

build_all


# **FIXME**  (automate this)
# generate two blank ext4 filesystems (so we don't have to format on target)
dd if=/dev/zero of=install/soc_cv1813h_milkv_duos_ota_sd/placeholder_staging.ext4 bs=1k count=300000
mkfs.ext4 install/soc_cv1813h_milkv_duos_ota_sd/placeholder_staging.ext4 

dd if=/dev/zero of=install/soc_cv1813h_milkv_duos_ota_sd/placeholder_data.ext4 bs=1k count=50000
mkfs.ext4 install/soc_cv1813h_milkv_duos_ota_sd/placeholder_data.ext4 

./install/soc_cv1813h_milkv_duos_ota_sd/placeholder_data.ext4
./install/soc_cv1813h_milkv_duos_ota_sd/placeholder_staging.ext4

# generate the sd image file
pack_sd_image_gz

# alternatively generate compressed image file for distribution
pack_sd_image_gz

# burn image to sd
Add wpa_supplicant.conf to boot parition
```


------------------------------------
Build ZBot Main OS and perform OTA Updates
------------------------------------
```
source device/milkv-duos-sd/boardconfig.sh 
source build/milkvsetup.sh 
defconfig cv1813h_milkv_duos_sd
clean_all
build_all
pack_sd_image
gen_swu_ota <version>
tools/zbot_updater.py update 192.168.42.1 install/board/zbot.swu
```


------------------------------------
Parition Table
GPT/MBR Hybrid (Boot Partition UUID Set such that Mac/Windows will mount)
------------------------------------
Upgraded Genimage to v18 (Support UUID Partition Types and Improved Hybrid Support)
(GPT Partition Table)
├── /dev/mmcblk0p1 (boot) [EFI System Partition, FAT32, 128M, Bootable]
├── /dev/mmcblk0p2 (env) [Linux, 256K]
├── /dev/mmcblk0p3 (otafs) [Linux, EXT4, 125M]
├── /dev/mmcblk0p4 (rootfs) [Linux, 275M]
├── /dev/mmcblk0p5 (data) [Linux, 50M]
└── /dev/mmcblk0p6 (staging) [Linux, 300M]

U-Boot Configured to Support Persistent Environment (ext4 backend via env partition)

Added data parition to seperate from rootfs. (config, weights, data)

------------------------------------
OTA Environment Mechanics
------------------------------------
```
Key Variables
---
rootfs=root=/dev/mmcblk0p4 rootwait rw   -- default rootfs location
otafs=root=/dev/mmcblk0p3 rootwait rw    -- default otafs location
root=root=/dev/mmcblk0p4 rootwait rw      -- booted partition
next_boot = {otafs, rootfs}                           -- next boot partition

# example (boot into otafs)
fw_setenv next_boot otafs
reboot

boot-ota
boot-zbot
```

------------------------------------
OTA Updater
------------------------------------
```
# OTA CLI Tool
tools/zbot_updater.py update <host> <swu file>
```

SWUpdate Backend
- listens on port 10000 (via websocket and http)
- stages firmware, writes raw image to partition, writes files, updates microcontrollers
- handles hw compatibility, sw version rules, hash validation and compressed images.


---
sw-description (generated via gen_swu_ota)
---
```
software = {
    version = "1.01";
    description = "Zeroth01 v1.01 Update";
    hardware_compatibility = ["1.0", "1.1"];
    images = (
        {
          filename = "rootfs.ext4.gz";
          device = "/dev/mmcblk0p4";
          type = "raw";
          sha256 = "2a603bae7d73c95f4bff20f775a487c1e336430a21e525777a2516c7f73d5a5f";
          compressed = "zlib";
        }
    );
}
```

------------------------------------
Work in progress
------------------------------------
- Update utility triggers rootfs to reboot into otafs for updates
- LCD / Local UI for update process (show progress, ask confirm to update etc.)
- Checking/Downloading update from github/gitlab (Zeroth team feedback)
- Move this to BR_EXTERNAL (shouldn't be entangled in buildroot source tree direclty)
- Add redundancy (u-boot env,  otafs a/b, u-boot a/b) (ordered for priority)
- Security
