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

#define AUX_MUTE        0x80
#define VOICE_MUTE      0x80
#define VOICE_VOL_MASK  0x3F
#define AUX_VOL_MASK    0x1F
#define MIC_BOOST       0x20
#define REC_VOL_MASK    0x0F
#define REC_SRC_MASK    0xC0

void wss_indirectRegWrite(u16 port, u8 idxReg, u8 value) {
    outportb(port+4, idxReg);
    outportb(port+5, value); 
}

u8 wss_indirectRegRead(u16 port, u8 idxReg) {
    outportb(port+4, idxReg);
    return inportb (port+5);
}

bool wss_isAccessible(u16 port) {
    u8 tst = wss_indirectRegRead(port, 0x0C);
    return tst != 0xFF;
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
    u8 l = wss_indirectRegRead(port, 0) & ~REC_SRC_MASK;
    u8 r = wss_indirectRegRead(port, 1) & ~REC_SRC_MASK;

    assert (source < WSS_INPUT_COUNT);

    wss_indirectRegWrite(port, 0, l | (source << 6));
    wss_indirectRegWrite(port, 1, r | (source << 6));
}

u8 wss_mixer_getInputSource (u16 port) {
    u8 l = wss_indirectRegRead(port, 0) & REC_SRC_MASK;
    u8 r = wss_indirectRegRead(port, 1) & REC_SRC_MASK;

    /* we only support both channels on identical source */
    assert (l == r);

    return (l >> 6);
}

void wss_mixer_setMonitorVol  (u16 port, const wss_vol *vol) {
    /* Compared to regular mute bits, this is an *enable* flag! */
    u8 v = (vol->mute ? 0x00 : 0x01) | ((63 - vol->l) << 2);
    wss_indirectRegWrite(port, 0x0D, v);
}

/* Voice vol is attenuation, not gain, so 63 - x is used here */
void wss_mixer_setVoiceVol   (u16 port, const wss_vol *vol) {
    u8 lreg = (vol->mute ? VOICE_MUTE : 00);// | (WSS_VOL_MAX - vol->l);
    u8 rreg = (vol->mute ? VOICE_MUTE : 00);// | (WSS_VOL_MAX - vol->r);

    /* volume */
    
    lreg |= (WSS_VOL_MAX - vol->l);
    rreg |= (WSS_VOL_MAX - vol->r);
    
    wss_indirectRegWrite(port, 6, lreg);
    wss_indirectRegWrite(port, 7, rreg);
}

void wss_mixer_getVoiceVol   (u16 port, wss_vol *vol) {
    u8 lreg = wss_indirectRegRead(port, 6);
    u8 rreg = wss_indirectRegRead(port, 7);

    bool lmute = (lreg & VOICE_MUTE) != 0;
    bool rmute = (rreg & VOICE_MUTE) != 0;

    assert (lmute == rmute);

    vol->mute = lmute || rmute;
    vol->l    = WSS_VOL_MAX - (lreg & VOICE_VOL_MASK);
    vol->r    = WSS_VOL_MAX - (rreg & VOICE_VOL_MASK);
}

void setAuxVolGeneric(u16 port, u8 lIdx, u8 rIdx, const wss_vol *vol) {
    u8 lreg = (vol->mute ? AUX_MUTE : 0x00);
    u8 rreg = (vol->mute ? AUX_MUTE : 0x00);

    /* volume, the higher the value, the quieter the sound */ 
    /* Range is 0 to 31, so we shift it) */
    lreg |= WSS_VOL_MAX - ((vol->l >> 1) & AUX_VOL_MASK);
    rreg |= WSS_VOL_MAX - ((vol->r >> 1) & AUX_VOL_MASK);

    wss_indirectRegWrite(port, lIdx, lreg);
    wss_indirectRegWrite(port, rIdx, rreg);
}

void getAuxVolGeneric(u16 port, u8 lIdx, u8 rIdx,       wss_vol *vol) {
    u8 lreg = wss_indirectRegRead(port, lIdx);
    u8 rreg = wss_indirectRegRead(port, rIdx);

    bool lmute = (lreg & AUX_MUTE) != 0;
    bool rmute = (rreg & AUX_MUTE) != 0; 

    /* Mute, only support both channels on same value */
    assert(lmute == rmute);
    vol->mute = lmute;

    /* volume, the higher the value, the quieter the sound */
    /* Range is 0 to 31, so we shift it) */
    vol->l = 63 - ((lreg & AUX_VOL_MASK) << 1);
    vol->r = 63 - ((rreg & AUX_VOL_MASK) << 1);

    /* This gain is 0-31 so we need to shift it left. 
       This would yield 62 for max volume so we set it to 63 in that case. */

    if (vol->l == 62) vol->l = 63;
    if (vol->r == 62) vol->r = 63;
}

void wss_mixer_setAux1Vol(u16 port, const wss_vol *vol) {
    setAuxVolGeneric(port, 2, 3, vol);
}

void wss_mixer_getAux1Vol(u16 port,       wss_vol *vol) {
    getAuxVolGeneric(port, 2, 3, vol);
}

void wss_mixer_setAux2Vol(u16 port, const wss_vol *vol) {
    setAuxVolGeneric(port, 4, 5, vol);
}

void wss_mixer_getAux2Vol(u16 port,       wss_vol *vol) {
    getAuxVolGeneric(port, 4, 5, vol);
}

void wss_mixer_setLineVol(u16 port, const wss_vol *vol) {
    setAuxVolGeneric(port, 18, 19, vol);
}

void wss_mixer_getLineVol(u16 port,       wss_vol *vol) {
    getAuxVolGeneric(port, 18, 19, vol);
}

void wss_mixer_setRecVol(u16 port, const wss_vol *vol) {
    u8 lreg = wss_indirectRegRead(port, 0);
    u8 rreg = wss_indirectRegRead(port, 1);

    lreg = (lreg & ~REC_VOL_MASK) | ((vol->l >> 2) & REC_VOL_MASK);
    rreg = (rreg & ~REC_VOL_MASK) | ((vol->r >> 2) & REC_VOL_MASK);

    if (vol->mute) {
        printf ("WARNING: Mute not supported on record\n");
    }

    wss_indirectRegWrite(port, 0, lreg);
    wss_indirectRegWrite(port, 1, rreg);
}

void wss_mixer_getRecVol(u16 port,       wss_vol *vol) {
    u8 lvol = wss_indirectRegRead(port, 0) & AUX_VOL_MASK;
    u8 rvol = wss_indirectRegRead(port, 1) & AUX_VOL_MASK;

    vol->mute = false;
    /* This gain is 0-15 so we need to shift it left. 
       This would yield 60 for max volume so we set it to 63 in that case. */
    vol->l = (lvol == 0x0F) ? WSS_VOL_MAX : (lvol << 2);
    vol->r = (rvol == 0x0F) ? WSS_VOL_MAX : (rvol << 2);
}

void wss_mixer_setMicBoost(u16 port, bool enable) {
    u8 lreg = wss_indirectRegRead(port, 0);
    u8 rreg = wss_indirectRegRead(port, 1);

    lreg = (lreg & ~MIC_BOOST) | (enable ? MIC_BOOST : 0x00);
    rreg = (rreg & ~MIC_BOOST) | (enable ? MIC_BOOST : 0x00);

    wss_indirectRegWrite(port, 0, lreg);
    wss_indirectRegWrite(port, 1, rreg);
}

bool wss_mixer_getMicBoost(u16 port) {
    u8 lreg = wss_indirectRegRead(port, 0) & MIC_BOOST;
    u8 rreg = wss_indirectRegRead(port, 1) & MIC_BOOST;

    assert (lreg == rreg);

    return lreg != 0;
}

void wss_mixer_muteVoice (u16 port, bool mute) {
    wss_vol vol;
    wss_mixer_getVoiceVol (port, &vol);
    vol.mute = true;
    wss_mixer_setVoiceVol (port, &vol);
}