#!/bin/bash
# test_blocks.sh

EXE="./build/my_filesystem"
CMDS="cmds_blocks.txt"

if [ ! -f "$EXE" ]; then
    echo "Error: Looking for $EXE... the executable does not exist!"
    echo "Make sure you have compiled the project with 'make' before running the test."
    exit 1
fi

echo "format tiny_blocks.img 32768" > $CMDS
echo "mount tiny_blocks.img /" >> $CMDS
echo "touch /big_file.txt" >> $CMDS

echo "Filling up data blocks..."

# A string of exactly 64 bytes. Appended 500 times, it equals 32,000 bytes.
# This will definitely saturate our tiny 32KB disk!
BLOCK="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"

for i in {1..500}; do
    echo 'append /big_file.txt "%s"\n' "$BLOCK" >> "$CMDS"
done

echo "df" >> $CMDS
echo "stat /big_file.txt" >> $CMDS
echo "exit" >> $CMDS

$EXE < $CMDS
rm $CMDS