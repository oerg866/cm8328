
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
#define WSS_INPUT_AUX1          1
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

bool wss_isAccessible         (u16 port);

void wss_setClockStereoReg    (u16 port, u8 value);
void wss_setMode2             (u16 port, bool enable);
void wss_setupCodec           (u16 port, bool stereo, bool pbEnable, bool recEnable);

void wss_mixer_setInputSource (u16 port, u8 source);
u8   wss_mixer_getInputSource (u16 port);

void wss_mixer_setMonitorVol  (u16 port, const wss_vol *vol);

void wss_mixer_setVoiceVol    (u16 port, const wss_vol *vol);
void wss_mixer_getVoiceVol    (u16 port,       wss_vol *vol);
void wss_mixer_setAux1Vol     (u16 port, const wss_vol *vol);
void wss_mixer_getAux1Vol     (u16 port,       wss_vol *vol);
void wss_mixer_setAux2Vol     (u16 port, const wss_vol *vol);
void wss_mixer_getAux2Vol     (u16 port,       wss_vol *vol);
void wss_mixer_setLineVol     (u16 port, const wss_vol *vol);
void wss_mixer_getLineVol     (u16 port,       wss_vol *vol);

void wss_mixer_setRecVol      (u16 port, const wss_vol *vol);
void wss_mixer_getRecVol      (u16 port,       wss_vol *vol);
void wss_mixer_setMicBoost    (u16 port, bool enable);
bool wss_mixer_getMicBoost    (u16 port);

void wss_mixer_muteVoice      (u16 port, bool mute);

void wss_setupCodec(u16 port, bool stereo, bool pbEnable, bool recEnable);

#endif /* WSS_H */
