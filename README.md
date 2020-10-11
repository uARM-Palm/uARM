# uARM

### What it is
uARM (based on my earlier project with the same name) is an ARM device emulator. Its features are:
 * cycle-inaccaurate emulation
 * vaguely correct emulation of SoC IP blocks
 * miserable-at-best performance on top-of-the-line modern hardware
 * questionable-at-best accuracy
 * somewhat comprehensible code
 * hacky-at-best quality
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
 * **-r <ROMFILE>** *Provide a NOR boot ROM. This is the OS image. Required, unless you supply the "-x" parameter*
 * **-x** *Tells the emulator that no NOR ROM exists (S3C24xx can boot directly from NAND, for example)*
 * **-n <NANDFILE>** *Provide a file for the initial state of the NAND flash. Required for devices that have NAND. You file needs to be of the proper size!*
 * **-s <SDCARDIMAGE>** *Provide an sdcard image. This is mutable (emulator can write to it). Cards under 2GB will appear as SD, larger as SDHC*
 * **-g <PORTNUMBER>** *Expect gdb to connect to a given port for debugging. Halt until it connects. The built-in GBD stub is quite good, supporting watchpoints, breakpoints, etc*

Examples:
```
    ./uARM -r PalmOsCobaltT3.img                          # Boot PalmOs Cobalt on the T|T3
    ./uARM -x -n Z22.NAND.bin                             # Boot Z22 from NAND
    ./uARM -r TX.NOR.bin -n TX.NAND.bin                   # Boot T|X
    ./uARM -r TE2.NOR.bin -n TE2.NAND.bin -s sdcard.img   # Boot T|E with an sd card
```

### Tested images
```
c145a13492de792538e3a3e11d1bfdeb3055e097ae0aafdb775760dcd2d819a3  AceecaPDA32.NAND.bin
95efc73fc8bb952eba86c8517ffd50ce025f9666ba94778cff59907802fd90f3  AximX3.NOR.bin
e6adb4efc3a6a5c5d2c110fad315ffa2a5f8c20d415e4b5a9b0eb39c181d7b5a  PalmTungstenC.NOR.bin
31bcdb9d48f45694ea316a3a51c0a7c43e0c46178cbb7c02a1fee07038e4acab  PalmTungstenE-fastboot.NOR.bin
419a61d5522cbebbc4d36c30a77ebd61af3720863e49c192939f516160513ef9  PalmTungstenE.NOR.bin
49d4008780831a4c024a9ead2f07adfb81f113cfcea248c51c49652fb0da3389  PalmTungstenT3-Cobalt.NOR.bin
4fb7d5abf5aa561b8cf16eab53104ef5500f0e8f13256dc17fde4a2cfab12568  PalmTungstenT3-OS5-fastboot.NOR.bin
118a324bbee341f1d3f704c7a10f61176ef8c2915a63489144776dbc8d9f1330  PalmTungstenT3-OS5.NOR.bin
6213b4f2fb1e92640ba29444fdf70a6a803932fda0a4b3c3824d5eb3af731ec0  PalmTX.NAND.bin
deab283c926d1f68d599b0f42bab7d84d2ff7dd7bb8ae41ec0dc381e6d7db530  PalmTX.NOR.bin.bin
14233cf431ba5c7ebbef57cd089ecde663d06b703e98115caa340b5bd65f76c1  PalmZ22.NAND.bin
3d2fe487662d438c60d4c83394337a49a570240b26cdf36ac114f17ceffdf0f7  PalmZire21.NOR.bin
35cb5dc218065886f0067b89daf3f09e7119a924410ff0157eac0591af26f215  PalmZire72-fastboot.NOR.bin.bin
f44dd41217682740b86714e77b5ca0ef41b1d0bed18361e80bcc833a40f6659b  PalmZire72.NOR.bin
16e22608d48a090857c93d264b5c80bcafc01c66bbd55679b1cfcda0cac673eb  PalmZireXYZ-fastboot.NOR.bin
12c2aa9a1ffdcfc40bf45896173aaa84b3a49d323ae25ccfaf11550c898847b8  PalmZireXYZ.NOR.bin
85e49365f4ddafe925edcc0cedf9e80cdc965fb5d352f1fce51b3581b820eff1  PalmZireZire31.NOR.bin
c6d5985cf183bae970e711430e51ae491088a5c173df555b9177e69ac860b94f  SonyTG50.NOR.bin
c67c54253013c08ea57e1c06c82a3209800c8b120e1be443300a39060c50c1ff  TungstenE2.NAND.bin
9537b1caa0d657e376cb68a4056f45057b41e09140c24873e7cf0fa781752c71  TungstenE2.NOR.bin
```
