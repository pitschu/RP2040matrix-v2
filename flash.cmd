
openocd_rp2040.exe -s "D:/_DevTools/openocd-0.11.0-3/share/openocd/scripts" -f "interface/picoprobe.cfg" -f "target/rp2040.cfg" -c "targets rp2040.core0" -c"program ./RP2040matrix.elf verify reset exit"
