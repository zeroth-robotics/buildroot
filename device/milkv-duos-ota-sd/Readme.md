 Zbot OTA Dev Setup
 ------------
 Setup build environment for zbot-ota-sd

source device/milkv-duos-ota-sd/boardconfig.sh 
source build/milkvsetup-ota.sh 
defconfig cv1813h_milkv_duos_ota_sd

clean_all
# run mkcvipary manually until this gets fixed (should just run during build_all)
python build/tools/common/image_tool/mkcvipart.py  build/boards/cv181x/cv1813h_milkv_duos_ota_sd/partition/partition_sd.xml u-boot-2021.10/include
build_all

# hack, need to add this to Makefile
# generate two blank ext4 filesystems (so we don't have to format on target)

dd if=/dev/zero of=install/soc_cv1813h_milkv_duos_ota_sd/placeholder_staging.ext4 bs=1k count=300000
mkfs.ext4 install/soc_cv1813h_milkv_duos_ota_sd/placeholder_staging.ext4 

dd if=/dev/zero of=install/soc_cv1813h_milkv_duos_ota_sd/placeholder_data.ext4 bs=1k count=50000
mkfs.ext4 install/soc_cv1813h_milkv_duos_ota_sd/placeholder_data.ext4 

./install/soc_cv1813h_milkv_duos_ota_sd/placeholder_data.ext4
./install/soc_cv1813h_milkv_duos_ota_sd/placeholder_staging.ext4

# generate the sd image file
pack_sd_image_gz

# optionally generate compressed image file
pack_sd_image_gz

# burn image (this is the base system without rootfs)
Add wpa_supplicant.conf to boot parition


------------------------------------
Build ZBot Main OS and perform OTA
------------------------------------

source device/milkv-duos-sd/boardconfig.sh 
source build/milkvsetup.sh 
defconfig cv1813h_milkv_duos_sd
clean_all
build_all
pack_sd_image
gen_swu_ota <version>

# ota update package install/board/zbot.swu

tools/zbot_updater.py update 192.168.42.1 install/board/zbot.swu