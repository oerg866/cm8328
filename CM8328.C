/*
 * C-Media CMI8328 DOS Init Driver
 * Driver Code
 *
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 *
 * Info and init sequence from Linux Kernel's cmi8328.c
 *    by Ondrej Zary <linux@rainbow-software.org>
 *
 */

#include <stdio.h>
#include <stddef.h>
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "TYPES.H"
#include "CM8328.H"
#include "ARGS.H"
#include "WSS.H"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#if !defined(outportb) && defined(_outp)
    #define outportb _outp
    #define inportb _inp
#endif

typedef struct {
    bool    sb_enable;
    bool    gp_enable;
    u16     sb_irq;
    u16     sb_dma;
    u16     sb_port;
    bool    mpu_enable;
    u16     mpu_irq;
    u16     mpu_port;
    char    cd_mode[ARG_MAX]; /* CDROM type string, such as Mitsumi */
    u16     cd_irq;
    u16     cd_dma;
    u16     cd_port;
} cm8328_cfg;

typedef struct {
    wss_vol o_voice;    /* Sample audio */
    wss_vol o_cd;       /* AUX1 on all cards with this chip = CD-Audio */
    wss_vol o_synth;    /* AUX2 on all cards with this chip = Synth (OPL3, maybe midi synth on DREAM cards?) */
    wss_vol o_line;     /* LINE input */
    wss_vol i_rec;
    u8      recSource;
    bool    micBoost;
} cm8328_mixer;

#define REG_CFG1 0x61
#define REG_CFG2 0x62
#define REG_CFG3 0x63

typedef struct {
    u8 cfg1;    /* 0x61 */
    u8 cfg2;    /* 0x62 */
    u8 cfg3;    /* 0x63 */
} cm8328_cfg_regs;

typedef struct {
    i16  value;     /* "Human" Value */
    u8   encoded;   /* Encoded register value */
    u8   mask;      /* Mask for decoding */
} reg_bits;

typedef struct {
    const char     *name;
    const reg_bits *regBits;
} cm8328_cdName;

typedef struct {
    const char     *name;
    u8              input;
} cm8328_wssInput;

static const reg_bits regSbEnable[] = {
/*    Value, Reg. val., Mask       Description              */
    {     0, 0x01 << 0, 0x01 }, /* Disabled: 1              */
    {     1, 0x00 << 0, 0x01 }, /* Enabled:  0              */
};

static const reg_bits regGameEnable[] = {
/*    Value, Reg. val., Mask       Description              */
    {     0, 0x00 << 1, 0x02 }, /* Disabled: 0              */
    {     1, 0x01 << 1, 0x02 }, /* Enabled:  1              */
};

static const reg_bits regSbIrqs[] = {
/*    Value, Reg. val., Mask       Description              */
    {     3, 0x01 << 2, 0x1C }, /* IRQ  3: 001              */
    {     5, 0x02 << 2, 0x1C }, /* IRQ  5: 010              */
    {     7, 0x03 << 2, 0x1C }, /* IRQ  7: 011              */
    {     9, 0x04 << 2, 0x1C }, /* IRQ  9: 100              */
    {    10, 0x05 << 2, 0x1C }, /* IRQ 10: 101              */
    {    11, 0x06 << 2, 0x1C }, /* IRQ 11: 110              */
};

static const reg_bits regSbDmas[] = {
/*    Value, Reg. val., Mask       Description              */
    {    -1, 0x00 << 5, 0x60 }, /* DISABLED: 00             */
    {     0, 0x01 << 5, 0x60 }, /* DMA 0: 01                */
    {     1, 0x02 << 5, 0x60 }, /* DMA 1: 10                */
    {     3, 0x03 << 5, 0x60 }, /* DMA 3: 11                */
};

static const reg_bits regSbPorts[] = {
/*    Value, Reg. val., Mask       Description              */
    { 0x220, 0x00 << 7, 0x80 }, /* Port 220: 0              */
    { 0x240, 0x01 << 7, 0x80 }, /* Port 240: 1              */
};

static const reg_bits regMpuEnable[] = {
/*    Value, Reg. val., Mask       Description              */
    {     0, 0x00 << 2, 0x04 }, /* Disabled: 0              */
    {     1, 0x01 << 2, 0x04 }, /* Enabled:  1              */
};

static const reg_bits regCdModes[] = {
/*    Value, Reg. val., Mask       Description              */
    {     0, 0x00 << 0, 0x03 }, /* Disabled:             00 */
    {     1, 0x01 << 0, 0x03 }, /* Panasonic:            01 */
    {     2, 0x01 << 0, 0x03 }, /* Sony/Mitsumi/Wearnes: 10 */
    {     3, 0x01 << 0, 0x03 }, /* IDE:                  11 */
};

static const reg_bits regMpuIrqs[] = {
/*    Value, Reg. val., Mask       Description              */
    {     3, 0x00 << 3, 0x18 }, /* IRQ 3: 00                */
    {     5, 0x01 << 3, 0x18 }, /* IRQ 5: 01                */
    {     7, 0x02 << 3, 0x18 }, /* IRQ 7: 10                */
    {     9, 0x03 << 3, 0x18 }, /* IRQ 9: 11                */
};

static const reg_bits regMpuPorts[] = {
/*    Value, Reg. val., Mask       Description              */
    { 0x300, 0x00 << 5, 0xE0 }, /* Port 300: 000            */
    { 0x310, 0x01 << 5, 0xE0 }, /* Port 310: 001            */
    { 0x320, 0x02 << 5, 0xE0 }, /* Port 320: 010            */
    { 0x330, 0x03 << 5, 0xE0 }, /* Port 330: 011            */
    { 0x332, 0x04 << 5, 0xE0 }, /* Port 332: 100            */
    { 0x334, 0x05 << 5, 0xE0 }, /* Port 334: 101            */
    { 0x336, 0x06 << 5, 0xE0 }, /* Port 336: 110            */
};

static const reg_bits regCdIrqs[] = {
/*    Value, Reg. val., Mask       Description              */
    {     0, 0x00 << 0, 0x07 }, /* DISABLE: 000             */
    {     3, 0x01 << 0, 0x07 }, /* IRQ  3:  001             */
    {     5, 0x02 << 0, 0x07 }, /* IRQ  5:  010             */
    {     7, 0x03 << 0, 0x07 }, /* IRQ  7:  011             */
    {     9, 0x04 << 0, 0x07 }, /* IRQ  9:  100             */
    {    10, 0x05 << 0, 0x07 }, /* IRQ 10:  101             */
    {    11, 0x06 << 0, 0x07 }, /* IRQ 11:  110             */
};

static const reg_bits regCdDmas[] = {
/*    Value, Reg. val., Mask       Description              */
    {    -1, 0x00 << 3, 0x18 }, /* DISABLED: 00             */
    {     0, 0x01 << 3, 0x18 }, /* DMA 0:    01             */
    {     1, 0x02 << 3, 0x18 }, /* DMA 1:    10             */
    {     3, 0x03 << 3, 0x18 }, /* DMA 2:    11             */
};

static const reg_bits regCdPorts[] = {
/*    Value, Reg. val., Mask       Description              */
    { 0x300, 0x00 << 5, 0xE0 }, /* Port 300: 000            */
    { 0x310, 0x01 << 5, 0xE0 }, /* Port 310: 001            */
    { 0x320, 0x02 << 5, 0xE0 }, /* Port 320: 010            */
    { 0x330, 0x03 << 5, 0xE0 }, /* Port 330: 011            */
    { 0x340, 0x04 << 5, 0xE0 }, /* Port 340: 100            */
    { 0x350, 0x05 << 5, 0xE0 }, /* Port 350: 101            */
    { 0x360, 0x06 << 5, 0xE0 }, /* Port 360: 110            */
    { 0x370, 0x07 << 5, 0xE0 }, /* Port 370: 111            */
};

static const cm8328_cdName cdNames[] = {
    { "Disabled",                   &regCdModes[0] },
    { "Panasonic",                  &regCdModes[1] },
    { "Mitsumi / Sony / Wearnes",   &regCdModes[2] },   /* little hack - this is the name that will be displayed after lookup. */
    { "Mitsumi",                    &regCdModes[2] },
    { "Sony",                       &regCdModes[2] },
    { "Wearnes",                    &regCdModes[2] },
    { "IDE",                        &regCdModes[3] },
};

static const cm8328_wssInput wssInputs[] = {
    { "LINE", WSS_INPUT_LINE      },
    { "CD",   WSS_INPUT_AUX1      },
    { "MIC",  WSS_INPUT_MIC       },
    { "LOOP", WSS_INPUT_WHATUHEAR },
};

static const u16 validBasePorts[] = {
    0x530, 0xE80, 0xF40, 0x604,
};

static u16          s_basePort      = 0x0000;
static cm8328_cfg   s_config;
static cm8328_mixer s_mixer;
static bool         s_init          = false;
static u8           s_tempReg       = 0;

/* Hack because for WSS mixer access SB needs to be disabled .... */

#define MIXER_ACCESS(port, _ACCESS_) {                \
                                     mixerAccessPre(port);  \
                                     _ACCESS_;              \
                                     mixerAccessPost(port); \
                                     }

#define FILL_WSS_VOL_STRUCT(s, _mute, _l, _r) { s.mute = _mute; s.l = _l; s.r = _r; }


#define DRIVER_VERSION "0.8"

static const char s_headerString[] =
    "CMI8328 Init Driver - Version " DRIVER_VERSION "\n"
    "  (C) 2023 Eric Voirin (oerg866@googlemail.com)\n"
    "  Discord: oerg866, twitter: @oerg866\n";

static u8 cfgRead(u16 port, u8 reg) {
    outportb(port + 3, 0x43);
    outportb(port + 3, 0x21);
    outportb(port + 3, reg);
    return inportb(port);
}

static void cfgWrite(u16 port, u8 reg, u8 value) {
    outportb(port + 3, 0x43);
    outportb(port + 3, 0x21);
    outportb(port + 3, reg);
    outportb(port + 3, value);
}

/* 
    Look up reg_bits for given value and set (OR) them to the u8 pointed to by dst.
    dst can be NULL, in which case this can be used as a "does x exist / is x valid" call.
       Return value is whether or not it was found.
*/
static bool regBitsLookupAndSet(const reg_bits *regBits, size_t size, i16 value, u8 *dst) {
    size_t idx;
    for (idx = 0; idx < size; ++idx) {
        if (regBits[idx].value == value) {
           if (dst != NULL) *dst |= regBits[idx].encoded;
           return true;
        }
    }
    return false;
}

static bool regBitsLookupAndSetCdromMode(const char *modeName, u8 *dst) {
    size_t idx ;
    for (idx = 0; idx < ARRAY_SIZE(cdNames); ++idx) {
        if (stricmp(modeName, cdNames[idx].name) == 0) {
            if (dst != NULL) *dst |= cdNames[idx].regBits->encoded;
            return true; 
        }
    }
    return false;
}

static bool regBitsDecodeAndGet(const reg_bits *regBits, size_t size, u8 reg, i16 *dst) {
    size_t idx;
    for (idx = 0; idx < size; ++idx) {
        if ((reg & regBits[idx].mask) == regBits[idx].encoded) {
           if (dst != NULL) *dst = regBits[idx].value;
           return true;
        }
    }
    printf("fail");
    abort();
    return false;
}

static bool regBitsDecodeAndGetCdromMode(u8 reg, char *dst) {
    size_t idx;
    for (idx = 0; idx < ARRAY_SIZE(cdNames); ++idx) {
        if ((reg & cdNames[idx].regBits->mask) == cdNames[idx].regBits->encoded) {
           if (dst != NULL) strcpy(dst, cdNames[idx].name);
           return true;
        }
    }
    printf("fail");
    abort();
    return false;
}

/* so we don't have to write ARRAY_SIZE manually every time... */
#define _regBitsLookupAndSet(regBits, value, dst) (regBitsLookupAndSet(regBits, ARRAY_SIZE(regBits), value, dst))
#define _regBitsDecodeAndGet(regBits, reg,   dst) (regBitsDecodeAndGet(regBits, ARRAY_SIZE(regBits), reg,   dst))

static void printConfig(const cm8328_cfg *cfg) {
    printf("------------------------------------------------------------------\n");
    printf("Sound Blaster Enable: %s\n", cfg->sb_enable       ? "Yes" : "No");
    printf("Gameport Enable:      %s\n", cfg->gp_enable       ? "Yes" : "No");
    printf("MPU-401 Enable:       %s\n", cfg->mpu_enable      ? "Yes" : "No");
    printf("CD-ROM Mode:          %s\n", cfg->cd_mode);
    printf("\n");

    if (cfg->sb_enable)
        printf("  SB  Port: 0x%03x   SB  IRQ: %2u     SB  DMA: %1u\n",                cfg->sb_port,  cfg->sb_irq,  cfg->sb_dma);
    if (cfg->mpu_enable)
        printf("  MPU Port: 0x%03x   MPU IRQ: %2u                 \n",                cfg->mpu_port, cfg->mpu_irq);
    if (strcmp(cfg->cd_mode, "Disabled") != 0)
        printf("  CD  Port: 0x%03x   CD  IRQ: %2u     CD  DMA: %1u    CD MODE: %s\n", cfg->cd_port,  cfg->cd_irq,  cfg->cd_dma, cfg->cd_mode);

    printf("\n  OPL3 'compatible' synthesizer at Port 0x388\n");

    if (cfg->sb_enable) {
        printf("\nYou may set the BLASTER variable as follows:\n");
        printf("   SET BLASTER=A%03x I%u D%u ", cfg->sb_port, cfg->sb_irq, cfg->sb_dma);
        if (cfg->mpu_enable)
            printf("P%03x ", cfg->mpu_port);
        printf("T4\n"); /* Always SBPro 2.0 */
    }
}

static void printVolumeLine(const char *str, const wss_vol *vol) {
    u8 i;

    printf("%s", str);

    fflush(stdout);

    for(i = 0; i < vol->l; ++i)
        putch(0xB2);

    for(i = vol->l; i < WSS_VOL_MAX; ++i)
        putch(0xB0);

    if (vol->mute) {
        printf(" [MUTE]\n");
    } else {
        printf(" (%02u)\n", vol->l);
    }
}

static void printMixer(const cm8328_mixer *mixer) {
    printf("\nMixer settings: \n");

    assert(mixer->recSource < WSS_INPUT_COUNT);

    printVolumeLine("VOICE >", &(mixer->o_voice));
    printVolumeLine("CD-IN >", &(mixer->o_cd));
    printVolumeLine("SYNTH >", &(mixer->o_synth));
    printVolumeLine("LINE  >", &(mixer->o_line));
    printVolumeLine("REC   <", &(mixer->i_rec));
    printf("\nRecord source: %s\n", wssInputs[mixer->recSource].name);
    printf("Mic Boost +20dB: %s", mixer->micBoost ? "Enabled" : "Disabled");
}

/* 
    Prepare WSS Mixer accesss. Needs to be called before any attempt to access the mixer is made. 
       This card is a weird mess. Because once you set the thing to SB mode, WSS is inaccessible unless you
       do this weird procedure to restore it. 

       Related comment from linux source: WSS dies when SB disable bit is cleared.

       Not the whole truth, but not far from it, either...

       This sequence was reverse engineered from the official MIX.COM mixer program.
*/
static void mixerAccessPre(u16 port) {
    u8 tmp = cfgRead(port, REG_CFG1);

    s_tempReg = tmp;

    tmp &= 0xFE;
    cfgWrite     (port, REG_CFG1, tmp);

    tmp = cfgRead(port, REG_CFG1);
    cfgWrite     (port, REG_CFG1, tmp | 0x01); 

    assert (wss_indirectRegRead(port, 0x0C) != 0xFF);

    wss_setMode2            (port, true);

/*
    cfgWrite     (port, REG_CFG1, tmp | 0x01); 

    tmp = cfgRead(port, REG_CFG1);
    cfgWrite     (port, REG_CFG1, (tmp & 0x0F) | 0x01); 

    tmp = cfgRead(port, REG_CFG1);
    cfgWrite     (port, REG_CFG1, (tmp & 0x0F) | 0x01); 
*/

}

/*
    Cleanup after WSS Mixer access. Needs to be called after mixer access to restore SB functionality if enabled.
*/
static void mixerAccessPost(u16 port) {
    wss_setMode2 (port, false);
    cfgWrite     (port, REG_CFG1, s_tempReg);
}

/*  Disables "voice filter" - which is really just a fancy name for running the codec at a higher frequency. */
static void disableVoiceFilter (u16 port) {
    mixerAccessPre          (port);
    wss_setClockStereoReg   (port, 0x16);
    mixerAccessPost         (port);
}

/* Encodes a config struct into the three hardware registers */
static bool encodeConfig(const cm8328_cfg *cfg, cm8328_cfg_regs *encoded) {
    bool ok = true;

    memset(encoded, 0, sizeof(cm8328_cfg_regs));

    ok &= _regBitsLookupAndSet(regSbEnable,   cfg->sb_enable,  &(encoded->cfg1));
    ok &= _regBitsLookupAndSet(regGameEnable, cfg->gp_enable,  &(encoded->cfg1));
    ok &= _regBitsLookupAndSet(regSbIrqs,     cfg->sb_irq,     &(encoded->cfg1));
    ok &= _regBitsLookupAndSet(regSbDmas,     cfg->sb_dma,     &(encoded->cfg1));
    ok &= _regBitsLookupAndSet(regSbPorts,    cfg->sb_port,    &(encoded->cfg1));

    ok &= regBitsLookupAndSetCdromMode       (cfg->cd_mode,    &(encoded->cfg2));
    ok &= _regBitsLookupAndSet(regMpuEnable,  cfg->mpu_enable, &(encoded->cfg2));
    ok &= _regBitsLookupAndSet(regMpuIrqs,    cfg->mpu_irq,    &(encoded->cfg2));
    ok &= _regBitsLookupAndSet(regMpuPorts,   cfg->mpu_port,   &(encoded->cfg2));

    ok &= _regBitsLookupAndSet(regCdIrqs,     cfg->cd_irq,     &(encoded->cfg3));
    ok &= _regBitsLookupAndSet(regCdDmas,     cfg->cd_dma,     &(encoded->cfg3));
    ok &= _regBitsLookupAndSet(regCdPorts,    cfg->cd_port,    &(encoded->cfg3));

    return ok;
}

static bool decodeConfig (cm8328_cfg *cfg, const cm8328_cfg_regs *encoded) {
    i16 tmp;
    bool ok = true;

    ok &= _regBitsDecodeAndGet(regSbEnable,   encoded->cfg1,   &tmp); cfg->sb_enable  = tmp;
    ok &= _regBitsDecodeAndGet(regGameEnable, encoded->cfg1,   &tmp); cfg->gp_enable  = tmp;
    ok &= _regBitsDecodeAndGet(regSbIrqs,     encoded->cfg1,   &tmp); cfg->sb_irq     = tmp;
    ok &= _regBitsDecodeAndGet(regSbDmas,     encoded->cfg1,   &tmp); cfg->sb_dma     = tmp;
    ok &= _regBitsDecodeAndGet(regSbPorts,    encoded->cfg1,   &tmp); cfg->sb_port    = tmp;

    ok &= regBitsDecodeAndGetCdromMode (encoded->cfg2, &cfg->cd_mode);
    ok &= _regBitsDecodeAndGet(regMpuEnable,  encoded->cfg2,   &tmp); cfg->mpu_enable = tmp;
    ok &= _regBitsDecodeAndGet(regMpuIrqs,    encoded->cfg2,   &tmp); cfg->mpu_irq    = tmp;
    ok &= _regBitsDecodeAndGet(regMpuPorts,   encoded->cfg2,   &tmp); cfg->mpu_port   = tmp;

    ok &= _regBitsDecodeAndGet(regCdIrqs,     encoded->cfg3,   &tmp); cfg->cd_irq     = tmp;
    ok &= _regBitsDecodeAndGet(regCdDmas,     encoded->cfg3,   &tmp); cfg->cd_dma     = tmp;
    ok &= _regBitsDecodeAndGet(regCdPorts,    encoded->cfg3,   &tmp); cfg->cd_port    = tmp;

    return ok;
}

/* Attempts to initialize the card. */
static bool initCard (u16 port) {
    bool ok;

    /* Official driver first sets SB disabled, then clears disable bit.
       Doing this to maximize compatibility... */

    /* Initialize WSS codec (TODO: Make this more elegant)
    /* Info from CS4231 codec datasheet & reverse engineering official driver */

    /* firstly verify we can access it */
    mixerAccessPre(port);
    ok = wss_isAccessible(port);
    mixerAccessPost(port);

    /* now set up the codec and initial mixer output */ 

    MIXER_ACCESS(port, wss_setupCodec(port, true, true, true));

    return ok;
}

static bool applyConfig (u16 port, const cm8328_cfg * cfg) {
    cm8328_cfg_regs regs;
    bool            success = encodeConfig(cfg, &regs);

    if (!success) {
       printf("An invalid configuration value was detected. Aborting...\n");
       return false;
    }

    cfgWrite(port, REG_CFG1, regs.cfg1 | 0x01 ); 
    cfgWrite(port, REG_CFG1, regs.cfg1        );  /* official driver writes twice */
    cfgWrite(port, REG_CFG1, regs.cfg1        );  /* don't ask me why... */

    cfgWrite(port, REG_CFG2, regs.cfg2);
    cfgWrite(port, REG_CFG3, regs.cfg3);

    /* last indication of success: written config matches */

    success &= (cfgRead(port, REG_CFG1) == regs.cfg1);
    success &= (cfgRead(port, REG_CFG2) == regs.cfg2);
    success &= (cfgRead(port, REG_CFG3) == regs.cfg3);

    if (!success) {
        printf("Setting CMI8328 configuration failed!\n");
        return false;
    }

    disableVoiceFilter(port);

    return success;
}

static bool applyMixer(u16 port, const cm8328_mixer *mixer) {
    bool ok;

    mixerAccessPre(port);

    ok = wss_isAccessible(port);

    wss_mixer_setVoiceVol   (port, &(mixer->o_voice));
    wss_mixer_setAux1Vol    (port, &(mixer->o_cd));
    wss_mixer_setAux2Vol    (port, &(mixer->o_synth));
    wss_mixer_setLineVol    (port, &(mixer->o_line));
    wss_mixer_setRecVol     (port, &(mixer->i_rec));

    wss_mixer_setInputSource(port, mixer->recSource);
    wss_mixer_setMicBoost   (port, mixer->micBoost);

    mixerAccessPost(port);

    return ok; /* TODO: error handling */
}

/* Tries to detect the card */
static bool findCard() {
    size_t i;
    u16    currentPort;

    for (i = 0; i < ARRAY_SIZE(validBasePorts); ++i) {
        currentPort = validBasePorts[i];

        printf("Attempting to find CMI8328 on port 0x%03x...\n", currentPort);

        if (cfgRead(currentPort, REG_CFG1) != 0xff) {
            printf("Card found!\n");
            s_basePort = currentPort;
            return true;
        }
    }

    return false;
}

static bool getCurrentConfig(u16 port, cm8328_cfg *dst) {
    cm8328_cfg_regs existingCfg;

    existingCfg.cfg1 = cfgRead(port, REG_CFG1);
    existingCfg.cfg2 = cfgRead(port, REG_CFG2);
    existingCfg.cfg3 = cfgRead(port, REG_CFG3);

    return decodeConfig(dst, &existingCfg);
}

static bool getCurrentMixer(u16 port, cm8328_mixer *dst) {
    bool ok;

    mixerAccessPre(port);

    ok = wss_isAccessible(port);

    wss_mixer_getVoiceVol   (port, &(dst->o_voice));
    wss_mixer_getAux1Vol    (port, &(dst->o_cd));
    wss_mixer_getAux2Vol    (port, &(dst->o_synth));
    wss_mixer_getLineVol    (port, &(dst->o_line));
    wss_mixer_getVoiceVol   (port, &(dst->i_rec));

    dst->recSource = wss_mixer_getInputSource(port);
    dst->micBoost  = wss_mixer_getMicBoost   (port);

    mixerAccessPost(port);

    return ok; /* TODO: error handling */
}

bool prepareDefaultCfg(const void *arg) {
    /* If requested, first make a default config.
       Gets called when /init argument is found.
       We have to do this here because later parameters will be ignored as this sets
       the defaults. */

    FILL_WSS_VOL_STRUCT(s_mixer.o_voice, false, 63, 63);
    FILL_WSS_VOL_STRUCT(s_mixer.o_line,  false, 48, 48);
    FILL_WSS_VOL_STRUCT(s_mixer.o_synth, false, 48, 48);
    FILL_WSS_VOL_STRUCT(s_mixer.o_cd,    false, 48, 48);
    FILL_WSS_VOL_STRUCT(s_mixer.i_rec,   false, 48, 48);
    
    s_mixer.micBoost            = false;
    s_mixer.recSource           = WSS_INPUT_LINE;

    s_config.sb_enable          = true;
    s_config.gp_enable          = true;
    s_config.sb_irq             = 5;
    s_config.sb_dma             = 1;
    s_config.sb_port            = 0x220;

    s_config.mpu_enable         = true;
    s_config.mpu_irq            = 9;
    s_config.mpu_port           = 0x330;

    sprintf(s_config.cd_mode,     "Disabled");

    s_config.cd_irq             = 3;      /* disabled, doesn't matter */
    s_config.cd_dma             = -1;     /* disabled, doesn't matter */
    s_config.cd_port            = 0x300;  /* disabled, doesn't matter */ 

    return true;
}

bool checkSbPort(const void *arg) {
    return _regBitsLookupAndSet(regSbPorts,  *((const u16*)arg), NULL);
}
bool checkSbIrq(const void *arg) {
    return _regBitsLookupAndSet(regSbIrqs,   *((const u8*) arg), NULL);
}
bool checkSbDma(const void *arg) {
    return _regBitsLookupAndSet(regSbDmas,   *((const u8*) arg), NULL);
}
bool checkMpuPort(const void *arg) {
    return _regBitsLookupAndSet(regMpuPorts, *((const u16*)arg), NULL);
}
bool checkMpuIrq(const void *arg) {
    return _regBitsLookupAndSet(regMpuIrqs,  *((const u8*) arg), NULL);
}
bool checkCdromPort(const void *arg) {
    return _regBitsLookupAndSet(regCdPorts,  *((const u16*)arg), NULL);
}
bool checkCdromIrq(const void *arg) {
    return _regBitsLookupAndSet(regCdIrqs,   *((const u8*) arg), NULL);
}
bool checkCdromDma(const void *arg) {
    return _regBitsLookupAndSet(regCdDmas,   *((const u8*) arg), NULL);
}
bool checkCdromMode(const void *arg) {
    return regBitsLookupAndSetCdromMode((const char *) arg, NULL);
}

bool setVolumeIfInRange(wss_vol *vol, const u8 *value) {
    if (*value <= WSS_VOL_MAX) {
        vol->l = *value;
        vol->r = *value;
        return true;
    }
    return false;
}

bool setVoiceVolume(const void *arg) {
    return setVolumeIfInRange(&s_mixer.o_voice, (const u8*) arg);
}
bool setCdVolume(const void *arg) {
    return setVolumeIfInRange(&s_mixer.o_cd,    (const u8*) arg);
}
bool setSynthVolume(const void *arg) {
    return setVolumeIfInRange(&s_mixer.o_synth, (const u8*) arg);
}
bool setLineVolume(const void *arg) {
    return setVolumeIfInRange(&s_mixer.o_line,  (const u8*) arg);
}
bool setAllVolumes(const void *arg) {
    return setVolumeIfInRange(&s_mixer.o_voice, (const u8*) arg)
        && setVolumeIfInRange(&s_mixer.o_cd,    (const u8*) arg)
        && setVolumeIfInRange(&s_mixer.o_synth, (const u8*) arg)
        && setVolumeIfInRange(&s_mixer.o_line,  (const u8*) arg);
}

bool setRecVolume(const void *arg) {
    return setVolumeIfInRange(&s_mixer.i_rec,   (const u8*) arg);
}

bool setRecSource(const void *arg) {
    size_t idx;
    for (idx = 0; idx < ARRAY_SIZE(wssInputs); ++idx) {
        printf("%s %s\n", arg, wssInputs[idx].name);
        if (strcmp(wssInputs[idx].name, (const char *) arg) == 0) {
            s_mixer.recSource = wssInputs[idx].input;
            return true;
        }
    }
    return false; 
}

static const args_arg validArgs[] = {
    ARGS_HEADER(s_headerString),

    /* INIT HACK: /init will call "prepareDefaultCfg", disguised as checker function */

    { "init",  "Initialize Card",      ARG_FLAG, &s_init,              prepareDefaultCfg },

    ARGS_EXPLAIN("NOTE: If used, this MUST be the first argument!"),
    ARGS_EXPLAIN("It will reset the card to the following defaults,"),
    ARGS_EXPLAIN("which the arguments following it can then alter."),
    ARGS_EXPLAIN("     SB Port  220h, IRQ5, DMA1, Game Port Enabled"),
    ARGS_EXPLAIN("     MPU Port 330h, IRQ9, CD-ROM Disabled"),

    { "sb",    "Sound Blaster Enable", ARG_BOOL, &s_config.sb_enable,  NULL },
    { "sbp",   "Sound Blaster Port",   ARG_U16,  &s_config.sb_port,    checkSbPort },
    { "sbi",   "Sound Blaster IRQ",    ARG_U8,   &s_config.sb_irq,     checkSbIrq  },
    { "sbd",   "Sound Blaster DMA",    ARG_U8,   &s_config.sb_dma,     checkSbDma  },
    { "gp",    "Game Port Enable",     ARG_BOOL, &s_config.gp_enable,  NULL },

    ARGS_BLANK,

    { "mpu",   "MPU401 Enable",        ARG_BOOL, &s_config.mpu_enable, NULL },
    { "mpup",  "MPU401 Port",          ARG_U16,  &s_config.mpu_port,   checkMpuPort },
    { "mpui",  "MPU401 IRQ",           ARG_U8,   &s_config.mpu_irq,    checkMpuIrq  },

    ARGS_BLANK,

    { "cd",    "CD-ROM Mode",          ARG_STR,  &s_config.cd_mode,    checkCdromMode },
    ARGS_EXPLAIN("Can be Disabled, Panasonic, Mitsumi, Sony, IDE"),

    { "cdp",   "CD-ROM Port",          ARG_U16,  &s_config.sb_port,    checkCdromPort },
    { "cdi",   "CD-ROM IRQ",           ARG_U8,   &s_config.sb_irq,     checkCdromIrq  },
    { "cdd",   "CD-ROM DMA",           ARG_U8,   &s_config.sb_dma,     checkCdromDma  },

    ARGS_BLANK,

    /* MIXER ARG HACK: The "checker" functions here are setters AND checkers. */

    { "Vv",    "Voice    Volume",      ARG_U8,   NULL,                 setVoiceVolume },
    { "Vm",    "Voice    Mute",        ARG_BOOL, &s_mixer.o_voice.mute,NULL           },
    { "Cv",    "CD Audio Volume",      ARG_U8,   NULL,                 setCdVolume    },
    { "Cm",    "CD Audio Mute",        ARG_BOOL, &s_mixer.o_cd.mute,   NULL           },
    { "Sv",    "Synth    Volume",      ARG_U8,   NULL,                 setSynthVolume },
    { "Sm",    "Synth    Mute",        ARG_BOOL, &s_mixer.o_synth.mute,NULL           },
    { "Lv",    "Line-In  Volume",      ARG_U8,   NULL,                 setLineVolume  },
    { "Lm",    "Line-In  Mute",        ARG_BOOL, &s_mixer.o_line.mute, NULL           },
    { "*v",    "ALL Output Volumes",   ARG_U8,   NULL,                 setAllVolumes  },

    ARGS_EXPLAIN("The above are volumes for the OUTPUT mixer."),

    { "Rv",    "Record   Volume",      ARG_U8,   NULL,                 setRecVolume   },

    ARGS_EXPLAIN("Volumes range from 0 to 63."),

    { "Rs",    "Record   Source",      ARG_STR,  NULL,                 setRecSource   },

    ARGS_EXPLAIN("Sources: LINE, CD, MIC, LOOP."),

    { "Rb",    "Mic +20dB Boost",      ARG_BOOL, &s_mixer.micBoost,    NULL           },

};

bool cm8328_prepare () {
    bool ok = true;

    /* Find the card */
    if (!findCard()) {
        printf("ERROR: Sound Card not detected!\n");
        return false;
    }

    /* Save its current config for later */
    memset(&s_config, 0, sizeof(cm8328_cfg));
    memset(&s_mixer,  0, sizeof(cm8328_mixer));

    ok  = getCurrentConfig(s_basePort, &s_config);
    ok &= getCurrentMixer(s_basePort, &s_mixer);

    return ok;
}


bool cm8328_parseArg (const char *arg) {
    return args_parseArg(validArgs, ARRAY_SIZE(validArgs), arg);
}

bool cm8328_configureCard () {
    bool ok = true;

    /* init the card if requested */
    if (s_init) { 
        ok &= initCard(s_basePort);

        if (!ok) {
            printf("ERROR initializing card and getting current configuration :( \n");
             return false;
        }
    }


    /* Apply the config parameters set by the user */ 
    ok &= applyConfig(s_basePort, &s_config);

    if (!ok) { 
        printf("ERROR applying card configuration... :( \n");
        return false;   /* Todo maybe improve error handling here */
    }


    /* Apply the mixer parameters set by the user */
    ok &= applyMixer(s_basePort, &s_mixer);

    if (!ok) { 
        printf("ERROR applying mixer settings... :( \n");
        return false;
    }

    ok &= getCurrentConfig(s_basePort, &s_config);
    ok &= getCurrentMixer (s_basePort, &s_mixer);

    if (!ok) {
        printf("ERROR reading card configuration back :(\n");
        return false;
    }

    printf("The card is currently configured as follows:\n");
    printConfig(&s_config);
    printMixer (&s_mixer);

    return true;
}
