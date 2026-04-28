#!/bin/bash
# test_inodes_full.sh

EXE="./build/my_filesystem"
IMG="tiny2.img"
CMDS="cmds_inodes.txt"

if [ ! -f "$EXE" ]; then
    echo "Error: Looking for $EXE... the executable does not exist!"
    echo "Make sure you have compiled the project with 'make' before running the test."
    exit 1
fi

echo "=== START INODE EXHAUSTION TEST ==="

echo "format $IMG 32768" > $CMDS
echo "mount $IMG /" >> $CMDS

echo "Creating 100 empty files..."

# We create empty files until the system runs out of free inodes and blocks us.
# For a 32KB disk with 64 total inodes, it should fail around file_63.txt.
for i in {1..100}; do
    echo "touch /file_$i.txt" >> $CMDS
done

echo "df" >> $CMDS
echo "exit" >> $CMDS

$EXE < $CMDS
rm $CMDS