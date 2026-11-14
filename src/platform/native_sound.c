/* Native silent sound backend; preserves the original engine API. */
#include "WL_DEF.H"

boolean AdLibPresent = false;
boolean SoundSourcePresent = false;
boolean SoundBlasterPresent = false;
boolean NeedsMusic = false;
boolean SoundPositioned = false;
SDMode SoundMode = sdm_Off;
SDSMode DigiMode = sds_Off;
SMMode MusicMode = smm_Off;
boolean DigiPlaying = false;
int DigiMap[256];
longword TimeCount;
static void (*sound_user_hook)(void);

void alOut(byte port, byte value) { (void)port; (void)value; }
void SD_Startup(void) { AdLibPresent=false; SoundSourcePresent=false; SoundBlasterPresent=false; NeedsMusic=false; SoundPositioned=false; SoundMode=sdm_Off; DigiMode=sds_Off; MusicMode=smm_Off; DigiPlaying=false; TimeCount=0; }
void SD_Shutdown(void) { DigiPlaying=false; SoundMode=sdm_Off; MusicMode=smm_Off; DigiMode=sds_Off; sound_user_hook=NULL; }
void SD_Default(boolean gotit, SDMode sd, SMMode sm) { (void)gotit; (void)sd; (void)sm; SoundMode=sdm_Off; MusicMode=smm_Off; DigiMode=sds_Off; NeedsMusic=false; }
void SD_PositionSound(int leftvol, int rightvol) { (void)leftvol; (void)rightvol; }
boolean SD_PlaySound(soundnames sound) { (void)sound; return false; }
void SD_SetPosition(int leftvol, int rightvol) { (void)leftvol; (void)rightvol; }
void SD_StopSound(void) {}
void SD_WaitSoundDone(void) {}
void SD_StartMusic(MusicGroup far *music) { (void)music; }
void SD_MusicOn(void) {}
void SD_MusicOff(void) {}
void SD_FadeOutMusic(void) {}
void SD_SetUserHook(void (*hook)(void)) { sound_user_hook=hook; }
boolean SD_MusicPlaying(void) { return false; }
boolean SD_SetSoundMode(SDMode mode) { SoundMode=sdm_Off; return mode==sdm_Off; }
boolean SD_SetMusicMode(SMMode mode) { MusicMode=smm_Off; return mode==smm_Off; }
word SD_SoundPlaying(void) { return 0; }
void SD_SetDigiDevice(SDSMode mode) { (void)mode; DigiMode=sds_Off; }
void SD_PlayDigitized(word which, int leftpos, int rightpos) { (void)which; (void)leftpos; (void)rightpos; DigiPlaying=false; }
void SD_StopDigitized(void) { DigiPlaying=false; }
void SD_Poll(void) { ++TimeCount; if (sound_user_hook != NULL) sound_user_hook(); }
