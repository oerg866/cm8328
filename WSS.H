
#ifndef WSS_H

#define WSS_H

/* TODO

#define WSS_FORMAT_PCM_U8       0
#define WSS_FORMAT_ULAW_8       1
#define WSS_FORMAT_PCM_S16LE    2
#define WSS_FORMAT_ALAW_8       3
#define WSS_FORMAT_INVALID      4
#define WSS_FORMAT_ADPCM_8      5
#define WSS_FORMAT_PCM_S16BE    6

*/


#define WSS_INPUT_LINE          0
#define WSS_INPUT_AUX           1
#define WSS_INPUT_MIC           2
#define WSS_INPUT_WHATUHEAR     3

#define WSS_INPUT_COUNT         4

#define WSS_MODE_CHANGE         0x40

/* 
   All volumes range from 0 to 63 for simplicity's sake,
   even though internally, the different inputs and outputs support
   different ranges. More granular amounts are therefore ignored for
   some of them.
*/

typedef struct {
    bool mute;
    u8   l;
    u8   r;
} wss_vol;

#define WSS_VOL_MAX             63


void wss_indirectRegWrite     (u16 port, u8 idxReg, u8 value);
u8   wss_indirectRegRead      (u16 port, u8 idxReg);

void wss_setClockStereoReg    (u16 port, u8 value);
void wss_setMode2             (u16 port, bool enable);
void wss_setupCodec           (u16 port, bool stereo, bool pbEnable, bool recEnable);

void wss_mixer_setInputSource (u16 port, u8 source);

void wss_mixer_setMicVol      (u16 port, const wss_vol *vol);
void wss_mixer_setAuxVol      (u16 port, const wss_vol *vol);
void wss_mixer_setLineVol     (u16 port, const wss_vol *vol);

void wss_mixer_setMonitorVol  (u16 port, const wss_vol *vol);

void wss_mixer_setOutputVol   (u16 port, const wss_vol *vol);
void wss_mixer_getOutputVol   (u16 port,       wss_vol *vol);

void wss_mixer_muteOutput     (u16 port, bool mute);

void wss_setupCodec(u16 port, bool stereo, bool pbEnable, bool recEnable);

#endif /* WSS_H */
