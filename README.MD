# CM8328

(C) 2023 Eric Voirin (oerg866)
---

`CM8328` is a DOS driver for **C-Media CM8328**-based ISA sound cards, such as the *Zoltrix Audio Plus 3200 V2 AV305* and the *Zoltrix Audio Plus 3200 Wave AV500*.

## Usage

There are many command line parameters. Check `CM8328.EXE /?` for info. Here are some examples:

* `CM8328.EXE`

    Displays the card's current configuration.

* `CM8328.EXE /init`

    Initializes the card with sensible defaults:
    * Sound Blaster (Port 220h, Interrupt 5, DMA 1)
    * Gameport Enabled
    * MPU-401 (Port 330h, Interrupt 9)
    * CD-ROM disabled
    * Voice volume: 63
    * All inputs playback volumes: 63

* `CM8328.EXE /init /sb:1 /sbp:0x220 /sbi:7 /sbd:1 /gp:0 /mpu:0`

    Initializes the card with the following values:
    * Sound Blaster (Port 220h, Interrupt 7, DMA 1)
    * Gameport disabled
    * MPU401 disabled
    * CD-ROM disabled
    * Default mixer settings (see above)

* `CM8328.EXE /Rs:CD`

    Sets record source to CD-Audio.

## Building
* Download and install the **OpenWatcom** C Compiler.
* Go to the repository folder
* Type `wmake` and press enter.
* That's it!

Note: you can also use Borland Turbo C using `MAKEFULE.TCC`.

## CM8328 Chip Notes
* The C-Media CM8328 is not a PnP audio device.
* The OPL3 is always enabled and resides at port 388h.
* The OPL3 implementation is messy, but not *offensively* bad. You can use it, but it's far from the original.
* The Linux driver says `WSS dies when SB Disable Bit is cleared`.

    Not the entire truth. The mixer only operates when SB is disabled, and the official `MIX.COM` utility manages to set it just fine.

    It does a (often) repeated read/write of this bit to restore functionality. See code.

## License

I have absolutely *no* clue about licenses, so I picked **CC-BY-NC 3.0**, which sounds like what I want. See **LICENSE** file.

TL;DR: Don't sell it, tell people who wrote it :)
