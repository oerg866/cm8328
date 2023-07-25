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
    bool sb_enable;
    bool gp_enable;
    u16  sb_irq;
    u16  sb_dma;
    u16  sb_port;
    bool mpu_enable;
    u16  mpu_irq;
    u16  mpu_port;
    char cd_mode[ARG_MAX]; /* first character of CDROM type string, such as Mitsumi */
    u16  cd_irq;
    u16  cd_dma;
    u16  cd_port;
} cm8328_cfg;

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
    { "Disabled",   &regCdModes[0] },
    { "Panasonic",  &regCdModes[1] },
    { "Mitsumi",    &regCdModes[2] },
    { "Sony",       &regCdModes[2] },
    { "Wearnes",    &regCdModes[2] },
    { "IDE",        &regCdModes[3] },
};

static const u16 validBasePorts[] = {
    0x530, 0xE80, 0xF40, 0x604,
};

static u16          s_basePort      = 0x0000;
static cm8328_cfg   s_config;
static bool         s_setBlaster    = false;
static bool         s_noInit        = false;
static u8           s_tempReg       = 0;

/* Hack because for WSS mixer access SB needs to be disabled .... */

#define MIXER_ACCESS(port, _ACCESS_) {                \
                                     mixerAccessPre(port);  \
                                     _ACCESS_;              \
                                     mixerAccessPost(port); \
                                     }


#define DRIVER_VERSION "0.1"

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

/* so we don't have to write ARRAY_SIZE manually every time... */
#define _regBitsLookupAndSet(regBits, value, dst) (regBitsLookupAndSet(regBits, ARRAY_SIZE(regBits), value, dst))

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

static void printConfig(const cm8328_cfg *cfg) {
    printf("Current Settings:\n");
    printf("------------------------------------------------------------------\n");
    printf("Sound Blaster Enable: %s\n", cfg->sb_enable       ? "Yes" : "No");
    printf("Gameport Enable:      %s\n", cfg->gp_enable       ? "Yes" : "No");
    printf("MPU-401 Enable:       %s\n", cfg->mpu_enable      ? "Yes" : "No");
    printf("CD-ROM Mode:          %s\n", cfg->cd_mode);
    printf("\n");

    printf("  SB Port: 0x%03x   MPU Port: 0x%03x    CD-ROM Port: 0x%03x \n", 
        cfg->sb_port, cfg->mpu_port, cfg->cd_port);
    printf("  SB IRQ:    %3u   MPU IRQ:    %3u    CD-ROM IRQ:    %3u \n", 
        cfg->sb_irq,  cfg->mpu_irq,  cfg->cd_irq);
    printf("  SB DMA:    %3u                      CD-ROM DMA:    %3u \n", 
        (cfg->sb_dma == -1) ? 0 : cfg->sb_dma,
        (cfg->cd_dma == -1) ? 0 : cfg->cd_dma);
    printf("\n");
    printf("  OPL3 'compatible' synthesizer at Port 0x388\n");
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

    cfgWrite     (port, REG_CFG1, tmp | 0x01); 

    tmp = cfgRead(port, REG_CFG1);
    cfgWrite     (port, REG_CFG1, (tmp & 0x0F) | 0x01); 

    tmp = cfgRead(port, REG_CFG1);
    cfgWrite     (port, REG_CFG1, (tmp & 0x0F) | 0x01); 
}

/*
    Cleanup after WSS Mixer access. Needs to be called after mixer access to restore SB functionality if enabled.
*/
static void mixerAccessPost(u16 port) {
    cfgWrite(port, REG_CFG1, s_tempReg);
}

/*  Disables "voice filter" - which is really just a fancy name for running the codec at a higher frequency. */
static void disableVoiceFilter (u16 port) {
    mixerAccessPre          (port);
    wss_setMode2            (port, true);
    wss_setClockStereoReg   (port, 0x16);
    wss_setMode2            (port, false);
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

/* TODO... need to think of an elegant design here
static bool decodeConfig (cm8328_cfg_regs *cfg, const cm8328_cfg_regs *encoded) {
    bool ok = true;

    ok &= _regBitsDecodeAndGet(regSbEnable,   encoded->cfg1,   &(cfg->sb_enable));
    ok &= _regBitsDecodeAndGet(regGameEnable, encoded->cfg1,   &(cfg->sb_enable));
    ok &= _regBitsDecodeAndGet(regSbIrqs,     encoded->cfg1,   &(cfg->sb_enable));
    ok &= _regBitsDecodeAndGet(regSbDmas,     encoded->cfg1,   &(cfg->sb_enable));
    ok &= _regBitsDecodeAndGet(regSbPorts,    encoded->cfg1,   &(cfg->sb_enable));
}
*/

static bool applyConfig (const cm8328_cfg * cfg) {
    cm8328_cfg_regs regs;
    wss_vol         defaultVolume = { false, WSS_VOL_MAX, WSS_VOL_MAX };
    bool            success = encodeConfig(cfg, &regs);

    printf("%02x %02x %02x\n", regs.cfg1, regs.cfg2, regs.cfg3);

    if (!success) {
       printf("An invalid configuration value was detected. Aborting...\n");
       return false;
    }

    printf("Attempting to set config:\n");

    printConfig(cfg);

    /* Official driver first sets SB disabled, then clears disable bit.
       Doing this to maximize compatibility... */

    /* Initialize WSS codec (TODO: Make this more elegant)
    /* Info from CS4231 codec datasheet & reverse engineering official driver */

    MIXER_ACCESS(s_basePort, wss_setupCodec(s_basePort, true, true, true));

    /* Make things hearable */
    MIXER_ACCESS(s_basePort, wss_mixer_setOutputVol(s_basePort, &defaultVolume));

    cfgWrite(s_basePort, REG_CFG1, regs.cfg1 | 0x01 ); 
    cfgWrite(s_basePort, REG_CFG1, regs.cfg1        );  /* official driver writes twice */
    cfgWrite(s_basePort, REG_CFG1, regs.cfg1        );  /* don't ask me why... */

    cfgWrite(s_basePort, REG_CFG2, regs.cfg2);
    cfgWrite(s_basePort, REG_CFG3, regs.cfg3);

    /* last indication of success: written config matches */

    success &= (cfgRead(s_basePort, REG_CFG1) == regs.cfg1);
    success &= (cfgRead(s_basePort, REG_CFG2) == regs.cfg2);
    success &= (cfgRead(s_basePort, REG_CFG3) == regs.cfg3);

    if (!success) {
        printf("Setting CMI8328 configuration failed!\n");
        return false;
    }

    disableVoiceFilter(s_basePort);

    return success;
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

bool checkSbPort(void *arg) {
    return _regBitsLookupAndSet(regSbPorts,  *((u16*)arg), NULL);
}
bool checkSbIrq(void *arg) {
    return _regBitsLookupAndSet(regSbIrqs,   *((u8*) arg), NULL);
}
bool checkSbDma(void *arg) {
    return _regBitsLookupAndSet(regSbDmas,   *((u8*) arg), NULL);
}
bool checkMpuPort(void *arg) {
    return _regBitsLookupAndSet(regMpuPorts, *((u16*)arg), NULL);
}
bool checkMpuIrq(void *arg) {
    return _regBitsLookupAndSet(regMpuIrqs,  *((u8*) arg), NULL);
}
bool checkCdromPort(void *arg) {
    return _regBitsLookupAndSet(regCdPorts,  *((u16*)arg), NULL);
}
bool checkCdromIrq(void *arg) {
    return _regBitsLookupAndSet(regCdIrqs,   *((u8*) arg), NULL);
}
bool checkCdromDma(void *arg) {
    return _regBitsLookupAndSet(regCdDmas,   *((u8*) arg), NULL);
}
bool checkCdromMode(void *arg) {
    return regBitsLookupAndSetCdromMode((const char *) arg, NULL);
}

static const args_arg validArgs[] = {
    ARGS_HEADER(s_headerString),

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

    { "set",   "Set BLASTER variable", ARG_FLAG, &s_setBlaster,        NULL },

    ARGS_EXPLAIN("Should only be used if SB is enabled."),

    { "noinit","Skip Card init",       ARG_FLAG, &s_noInit,            NULL },

    ARGS_EXPLAIN("Useful for mixer / volume settings."),
    ARGS_EXPLAIN("MIXER SETTINGS NOT IMPLEMENTED YET."),

    /* MIXER ARG HACK: The "checker" function here is really a setter. */

    /* TODO: add mixer args */
};

bool cm8328_prepare () {

    /* Setup default config */

    if (!findCard()) {
        printf("ERROR: Sound Card not detected!\n");
        return false;
    }

    memset(&s_config, 0, sizeof(cm8328_cfg));

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


bool cm8328_parseArg (const char *arg) {
    return args_parseArg(validArgs, ARRAY_SIZE(validArgs), arg);
}

bool cm8328_configureCard () {
    bool ok = true;

    if (!s_noInit) ok &= applyConfig(&s_config);

    if (!ok) { 
        printf("ERROR applying card configuration... :( \n");

        /* Todo maybe improve error handling here */
    }

    return ok;
}
