/*
 * Windows Sound System
 * Status and Mixer Control
 *
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */


#include "TYPES.H"

#include "WSS.H"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <conio.h>
#include <dos.h>

#if !defined(outportb) && defined(_outp)
    #define outportb _outp
    #define inportb _inp
#endif

typedef struct { 
    bool muted;
    u8   volume;
} wss_channel;


typedef struct {
    u8          val;
    const char *name;
} valName;

void wss_indirectRegWrite(u16 port, u8 idxReg, u8 value) {
    outportb(port+4, idxReg);
    outportb(port+5, value); 
}

u8 wss_indirectRegRead(u16 port, u8 idxReg) {
    outportb(port+4, idxReg);
    return inportb (port+5);
}

void wss_setClockStereoReg(u16 port, u8 value) {
    volatile u8 tst = 0;
    u8          i;

    wss_indirectRegRead (port, 0x48); 
    wss_indirectRegWrite(port, 0x48, value);

    for (i = 0; i < 16; ++i) {
        do { tst = inportb(port + 4); } while (tst == 0x80);
    }

    wss_indirectRegRead (port, 0x0B);
}

void wss_setMode2(u16 port, bool enable) {
    u8 val = wss_indirectRegRead(port, 0x0C);

    if (enable) {
        val |=  0x40;
    } else {
        val &= ~0x40;
    }

    wss_indirectRegWrite(port, 0x0C, val);

    printf("Mode 2 en %02x\n", wss_indirectRegRead(port, 0x0C));
}

static void waitForCalibrationDone(u16 port) {
    volatile u8 tst = wss_indirectRegRead(port, 0x0B);
    u8          i;

    for (i = 0; i < 16; ++i) {
        do { tst = inportb(port + 5); } while (tst & 0x20);
    }
}

void wss_setupCodec(u16 port, bool stereo, bool pbEnable, bool recEnable) {

    /* According to CS4231 datasheet:

        - Place CS4231A in Mode Change (base+4 bit 7)
        - Set CAL1, 0 in Interface Configuration Reg (I9)
        - Return from Mode Change by clearing base+4 bit7
        - Wait until ACI (in I11) is cleared
    */

    /* Mode change enable, Register 9 -> Capture PIO, PB PIO, Enable DAC calibration on mode change */

    u8 r8 = (stereo ? 0x10 : 0x00);
    u8 r9 = 0xC0 | (recEnable ? 0x02 : 0x00) | (pbEnable ? 0x01 : 0x00);

    wss_indirectRegWrite  (port, 0x49, 0xC8); /* Initially set calibration */

    wss_setClockStereoReg (port, r8);         /* write mono/stereo bit */

    waitForCalibrationDone(port);

    /* Mode change DISABLE, Register 9 */
    wss_indirectRegWrite(port, 0x09, r9);    /* no more calibration, set rec/pb mode */

}

void wss_mixer_setInputSource (u16 port, u8 source) {
    u8 l = wss_indirectRegRead(port, 0x00) & 0x3F;
    u8 r = wss_indirectRegRead(port, 0x01) & 0x3F;

    assert (source < WSS_INPUT_COUNT);

    wss_indirectRegWrite(port, 0x00, l | (source << 6));
    wss_indirectRegWrite(port, 0x01, r | (source << 6));
}

/*void wss_mixer_setMicVol      (u16 port, bool mute, u8 left, u8 right) {
//todo
}

void wss_mixer_setAuxVol      (u16 port, bool mute, u8 left, u8 right) {
//todo
}

void wss_mixer_setLineVol     (u16 port, bool mute, u8 left, u8 right) {
//todo
}*/

void wss_mixer_setMonitorVol  (u16 port, const wss_vol *vol) {
    /* Compared to regular mute bits, this is an *enable* flag! */
    u8 v = (vol->mute ? 0x00 : 0x01) | ((63 - vol->l) << 2);
    wss_indirectRegWrite(port, 0x0D, v);
}

void wss_mixer_setOutputVol   (u16 port, const wss_vol *vol) {
    wss_indirectRegWrite(port, 0x06, (vol->mute ? 0x80 : 00) | (63 - vol->l));
    wss_indirectRegWrite(port, 0x07, (vol->mute ? 0x80 : 00) | (63 - vol->r));
}

void wss_mixer_getOutputVol   (u16 port, wss_vol *vol) {
    u8 lreg = wss_indirectRegRead(port, 0x06);
    u8 rreg = wss_indirectRegRead(port, 0x07);

    bool lmute = (lreg & 0x80 == 0);
    bool rmute = (rreg & 0x80 == 0);

    assert (lmute == rmute);

    vol->mute = lmute || rmute;
    vol->l    = 63 - lreg & 0x3F;
    vol->r    = 63 - rreg & 0x3F;
}

void wss_mixer_muteOutput     (u16 port, bool mute) {
    wss_vol vol;
    wss_mixer_getOutputVol (port, &vol);
    vol.mute = true;
    wss_mixer_setOutputVol (port, &vol); 
}
