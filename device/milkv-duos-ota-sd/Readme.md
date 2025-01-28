 OTA Dev Setup
 ------------
 Setup build environment for milkv-duos-ota-sd

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

pack_sd_image

# optionally compress
gzip install/soc_cv1813h_milkv_duos_ota_sd/milkv-duos-ota-sd.img 

# burn image (this is the base system without rootfs)

#Done! add wpa_supplicant.conf to boot parition and then ready to update with kos and application


-----------------------
# create an swu update package for kos rootfs builds
#create a staging directory for update
mkdir zbot_v1-0

#copy rootfs.ext4 to staging
cp install/soc_cv1813h_milkv_duos_sd/rootfs.ext4 zbot_v1-0

#compress with gzip
gzip zbot_v1-0/rootfs.ext4

#generate sha256
sha256sum zbot_v1-0/rootfs.ext4.gz

# create and use reference swu-staging/sw-description in the new dir

#use zbot_updater to create the update package, and then use it to update the bot

tree of staging directory (only include sw-description any files referenced in it)
zbot_v1-0/
├── rootfs.ext4.gz
├── sw-description
└── zbot_zbot_v1-0.swu