# TFS

Simple filesystem to see how it works.
Data are stored in RAM.
Can create, open, read and write files and subdirectory.

```bash
make
mkdir bar/
touch image
dd bs=4096 count=100 if=/dev/zero of=image
mount -o loop -t tfs image bar/
dmesg # See kernel log
```