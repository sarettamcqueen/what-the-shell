#!/bin/bash
# test_max_filesize.sh

EXE="./build/my_filesystem"
CMDS="cmds_maxfile.txt"

if [ ! -f "$EXE" ]; then
    echo "Error: Looking for $EXE... the executable does not exist!"
    echo "Make sure you have compiled the project with 'make' before running the test."
    exit 1
fi

# Create a large disk (256 KB) so that physical disk space is not the bottleneck
echo "format maxfile.img 262144" > $CMDS
echo "mount maxfile.img /" >> $CMDS
echo "touch /huge_file.txt" >> $CMDS

echo "Writing data up to the inode architectural limit..."

# Generate a 1000-character string (the letter 'A' repeated 1000 times)
STRING=$(printf 'A%.0s' {1..1000})

# Append the string 75 times (75 * 1000 = 75,000 bytes).
# Our theoretical file limit is 71,680 bytes (12 direct + 128 indirect blocks * 512 bytes).
# Therefore, the last few appends must trigger a 'No space left on device' error!
for i in {1..75}; do
    echo "append /huge_file.txt \"$STRING\"" >> $CMDS
done

echo "stat /huge_file.txt" >> $CMDS
echo "df" >> $CMDS
echo "exit" >> $CMDS

$EXE < $CMDS
rm $CMDS