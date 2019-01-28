#! /bin/bash
sudo parted -s /dev/sdb mklabel gpt
sudo parted -s /dev/sdb unit s mkpart uboot 8192 16384
sudo parted -s /dev/sdb unit s mkpart recovery 16385 81920
sudo parted -s /dev/sdb unit s mkpart baseparamer 81921 83968
sudo parted -s /dev/sdb unit s mkpart resource 83969 114688
sudo parted -s /dev/sdb unit s mkpart kernel 114689 163840
sudo parted -s /dev/sdb unit s mkpart boot 163841 6167966
sudo mkfs -t ext4 /dev/sdb6
sudo mount -t ext4 -o loop /dev/sdb6 /mnt
cd ./buildroot/output/target
sudo cp * /mnt -rf
sync
sudo umount /mnt

