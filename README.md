# CH32V003DBG 

This repo contains a GDB-compatible remote debug interface for the RISC-V based CH32V003 series of chips by WinChipHead.
It allows you to program and debug a CH32V003 chip without needing the proprietary WCH-Link dongle or a modified copy of OpenOCD.
The app runs on a Raspberry Pi Pico and can communicate with the CH32V003 via WCH's semi-proprietary single-wire (SWIO) interface.
**This tool is very, very alpha - beware of bugs**

## Repo Structure

**src** is the main codebase. It compiles using the Pico SDK + CMake.
**src/singlewire.pio** is the Pico PIO code that generates SWIO waveforms on GP27

## Usage

Connect pin PD1 on your CH32V003 device to the Pico's SWIO pin (defaults to pin GP27).
Connect CH32V ground to Pico ground, and add a 1Kohm pull-up resistor from SWIO to +3.3v.
After that run "gdb-multiarch {your_binary.elf}" and type "target extended-remote /dev/ttyACM0" to connect to the debugger (replace ttyACM0 with whatever port your Pico shows up as).
Most operations should be faster than the WCH-Link by virtue of doing some basic Pico-side caching and avoiding redundant debug register read/writes.
Not all GDB remote functionality is implemented, but read/write of RAM, erasing/writing flash, setting breakpoints, and stepping should all work. The target chip can be reset via "monitor reset".

## Building:

Install the prerequisites:
```
sudo apt install cmake gcc-arm-none-eabi gcc-riscv64-unknown-elf xxd
```

Then run build.sh in the repo root. CMake should auto-fetch the Pico SDK as part of the build process.
Run "upload" to upload FW to your Pico if it's connected to a Pico Debug Probe, or just use the standard hold-reset-and-reboot to mount your Pico as a flash drive and then copy out/ch32v003dbg.uf2 to it.

## Modules

CH32V003DBG is broken up into a couple modules that can (in principle) be reused independently:

### swio
Implements the WCH SWIO protocol using the Pico's PIO block. Exposes a trivial get(addr)/put(addr,data) interface. Does not currently support "fast mode", but the standard mode runs at ~800kbps and should suffice.
Spec here - https://github.com/openwch/ch32v003/blob/main/RISC-V%20QingKeV2%20Microprocessor%20Debug%20Manual.pdf

### context
Exposes the various registers in the official RISC-V debug spec along with methods to read/write memory over the main bus and halt/resume/reset the CPU.
Spec here - https://github.com/riscv/riscv-debug-spec/blob/master/riscv-debug-stable.pdf 

### flash
Methods to read/write the CH32V003's flash. Most stuff hardcoded at the moment. WCHFlash does _not_ clobber device RAM, instead it streams data directly to the flash page buffer. This means that in theory you should be able to use it to replace flash contents without needing to reset the CPU, though I haven't tested that yet.
CH32V003 reference manual here - http://www.wch-ic.com/downloads/CH32V003RM_PDF.html

### break
The CH32V003 chip does _not_ support any hardware breakpoints. The official WCH-Link dongle simulates breakpoints by patching and unpatching flash every time it halts/resumes the processor. SoftBreak does something similar, but with optimizations to minimize the number of page updates needed. It also avoids page updates during the common 'single-step by setting breakpoints on every instruction' thing that GDB does, which makes stepping way faster.

### server
Communicates with the GDB host via the Pico's USB-to-serial port. Translates the GDB remote protocol into commands.
See "Appendix E" here for spec - https://sourceware.org/gdb/current/onlinedocs/gdb.pdf

### console
A trivial serial console exposed over the Pico’s USB CDC interface. It provides basic commands for debugging the debugger itself and simple device inspection.
Connect using a serial terminal such as `tio` (for example: `tio /dev/ttyACM*`) and type `help { boot | break | core | flash | info | options }` to list available commands.

### xmodem
Provides firmware upload via XMODEM-CRC or XMODEM-1K over a serial connection. Intended for simple, reliable flashing in bootloader or recovery setups where direct SWIO access is not used.
Firmware enters XMODEM mode upon receiving **SYN** (0x16). Status indications use the module's **RGB LED**:

- Erase/flash/verify failure: `cyan`
- 10× receive failure: `red`
- 10× send request exceeded: `blue`
- Flash is locked: `magenta`
- Success: `green`
