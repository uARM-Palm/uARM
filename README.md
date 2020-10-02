# uARM

### What it is
uARM (based on my earlier project with the same name) is an ARM device emulator. Its features are:
 * cycle-inaccaurate emulation
 * vaguely correct emulation of SoC IP blocks
 * miserable-at-best performance on top-of-the-line modern hardware
 * questionable-at-best accuracy
 * somewhat comprehensible code
 * contains a kind-of-cool virtual SD card emulator
 
 ### Why it is
  * No existing emulator properly emulates any PalmOS 5 device due to their variety and lack of hardware docs.
  * I promised [Emily](https://www.libretro.com/index.php/obituary-cuttlefish/) that I would do this
  * To help preservation efforts
  * To encourage others to contribute to improve these

### License
GPL v3. If you want another, contact me

### Supported Devices

| *DEVICE*           | *GRADE* | *WHAT DOESN'T WORK*                                                      | *SUGGESTED FUTURE WORK*                   |
|--------------------|---------|--------------------------------------------------------------------------|-------------------------------------------|
| Palm Z22           | A+      |                                                                          |                                           |
| Palm Zire 21       | A+      |                                                                          |                                           |
| Palm Zire 31       | A+      |                                                                          |                                           |
| Palm Tungsten E2   | A+      | <ul><li>Bluetooth</li></ul>                                              |                                           |
| Palm Tungsten T3   | A       | <ul><li>SD card</li></ul>                                                | SD support in W86L488                     |
| Palm Zire 71       | A       | <ul><li>Camera</li><li>SD card</li></ul>                                 | Camera, MMC support in OMAP               |
| Sony TG50          | A       | <ul><li>MS card</li><li>Bluetooth</li></ul>                              |                                           |
| Aceeca PDA32       | B       | <ul><li>Touch hangs sometimes</li><li>SD card</li></ul>                  | Touch, S3C2440's SD support               |
| Zire XYZ           | B       | <ul><li>Touch hangs sometimes</li><li>SD card</li></ul>                  | MMC support in OMAP, TSC210x improvements |
| Palm Tungsten E    | B       | <ul><li>Touch hangs sometimes</li><li>SD card</li></ul>                  | MMC support in OMAP, TSC210x improvements |
| Dell Axim X3       | C+      | Cannot get past tutorial. I am not a WM expert...                        | Sort out why WinMo gets stuck in tutorial |
| Palm Zire 72       | C       | <ul><li>Camera</li><li>Bluetooth</li><li>SD card</li><li>Touch</li></ul> | Sort out how PXA27x devices use WM9712    |
| Palm TX            | C       | <ul><li>WiFi</li><li>Bluetooth</li><li>SD card</li><li>Touch</li></ul>  | Sort out how PXA27x devices use WM9712    |
| Palm Tungsten C    | D-      | Does not boot                                                            | Sort out why                              |
| Palm Tungsten T/T2 | D-      | Does not boot: need to emulate the DSP                                   | Emulate the DSP                           |



Please note: 
 * Audio playback is currently not supported anywhere. Audio data is simply discarded. Wiring it up to audio out should not be hard, but it is not clear that the emulator is fast enough for this to be realtime
 * Emulators are currently stateless (except virtual SD card). Every start is like a fresh boot of a newly-erased device. This is true for NVFS devices too

### Emulated Hardware Details
Supported (for some definitions of the word) SoCs:
 * Intel PXA25x
 * Intel PXA26x
 * Intel PXA27x (including WMMX)
 * TI OMAP 31x (MMC support incomplete)
 * Samsung S3C2410
 * Samsung S3C2440

"Supported" misc chips:
 * Audio codecs / Touch controllers / ADCs (for touch and battery measuring, audio sent to /dev/null)
   * WM9705
   * WM9712
   * AK4534 (minimal as it only does audio)
   * AD7873
   * TSC2101 / TSC2102
   * ADS7846
 * PMICs / Charge controllers (minimal support to get PalmOs to boot
   * AN32502A
   * TPS6510
 * Memory card controllers
   * W86L488 (incomplete but close)
   * Sony's MS controller (just a few guesses with no chance for more, enough for PalmOS to boot)
 * Custom hardware
   * Sony's embeded controller in the PEG-TG50
   * Dell Axim X3's CPLD

### Host support
Any host with SDL2 support should work

### Building
Uncomment the proper device type in the makefile and run make. PGO is stongly recommended for a non-negligible speed boost.
Every build will support only one device type

### Running
A few command line options exist:
 * **-r <ROMFILE>** *Provide a NOR boot rom. This is the OS image. Required, unless you supply the "-x" paremeter*
 * **-x** *Tells the amulator that no boot rom exists (S3C24xx can boot directly form NAND, for example)*
 * **-n <NANDFILE>** *Provide a file for the initial state of the NAND flash. Required for devices that have NAND. You file needs to be of the proper size!*
 * **-s <SDCARDIMAGE>** *Provide an sdcard image. This is mutable (emulator can write to it). Cards under 2GB will appear as SD, larger as SDHC*
 * **-g <PORTNUMBER>** *Expect gdb to connect to a given port for debugging. Halt until it connects. the built-in GBD stub is quite good, supporting watchpoints, breakpoints, etc*

Examples:
```
    ./uARM -r PalmOsCobaltT3.img                          # Boot PalmOs Cobalt on the T|T3
    ./uARM -x -n Z22.NAND.bin                             # Boot Z22 from NAND
    ./uARM -n TX.NOR.bin -n TX.NAND.bin                   # Boot T|X
    ./uARM -n TE2.NOR.bin -n TE2.NAND.bin -s sdcard.img   # Boot T|E with an sd card
```
