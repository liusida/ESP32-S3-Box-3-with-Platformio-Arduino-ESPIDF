#!/bin/bash
# Usage: ./addr2line.sh "Backtrace: 0x4204318c:0x3fcebec0 0x4204321e:0x3fcebee0 ..."

# Path to your toolchain binary and ELF file
ADDR2LINE=~/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line
ELF=.pio/build/dictionary/firmware.elf

# Extract only the first hex in each "0xAAA:0xBBB" pair
ADDRS=$(echo "$1" | grep -o '0x[0-9a-f]\+' | awk 'NR % 2 == 1')

# Call addr2line with all the addresses
$ADDR2LINE -pfiaC -e $ELF $ADDRS
