#!/bin/bash

umount bar
rmmod tfs
insmod tfs.ko
mount -o loop -t tfs image bar/
dmesg
