/*
 * Native sound manager.
 *
 * The original PC-speaker effects are short streams of frequency commands
 * timed at 140 Hz. Convert those commands to mono PCM for the SDL3 playback
 * stream. AdLib music and digitised Sound Blaster/Sound Source playback remain
 * disabled until their respective synthesis/streaming backends are ported.
 */
#include "WL_DEF.H"
#include "platform.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#define NATIVE_PC_SAMPLE_RATE 22050u
#define NATIVE_PC_TICK_RATE (TickBase * 2u)
#define NATIVE_PC_AMPLITUDE 7000
#define NATIVE_PC_MAX_DURATION_TICKS (NATIVE_PC_TICK_RATE * 30u)

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
static soundnames current_sound;
static word current_priority;

static int16_t *build_pc_samples(const PCSound *sound, size_t *sample_count)
{
    uint64_t total_samples;
    uint64_t phase = 0;
    uint64_t sample_index;
    uint32_t tick;
    int16_t *samples;

    if (sound == NULL || sample_count == NULL ||
        sound->common.length == 0 ||
        sound->common.length > NATIVE_PC_MAX_DURATION_TICKS) {
        return NULL;
    }

    total_samples = ((uint64_t)sound->common.length *
        NATIVE_PC_SAMPLE_RATE) / NATIVE_PC_TICK_RATE;
    if (total_samples == 0 ||
        total_samples > (uint64_t)INT_MAX / sizeof(*samples)) {
        return NULL;
    }

    samples = (int16_t *)malloc((size_t)total_samples * sizeof(*samples));
    if (samples == NULL) {
        return NULL;
    }

    sample_index = 0;
    for (tick = 0; tick < sound->common.length; ++tick) {
        unsigned command = sound->data[tick];
        uint64_t first_sample =
            ((uint64_t)tick * NATIVE_PC_SAMPLE_RATE) / NATIVE_PC_TICK_RATE;
        uint64_t end_sample =
            ((uint64_t)(tick + 1u) * NATIVE_PC_SAMPLE_RATE) /
                NATIVE_PC_TICK_RATE;

        if (command == 0) {
            while (sample_index < end_sample) {
                samples[sample_index++] = 0;
            }
            phase = 0;
            continue;
        }

        {
            unsigned divisor = command * 60u;
            unsigned frequency = 1193180u / divisor;
            uint64_t phase_step;

            /* Limit inaudible/aliased timer settings to the useful band. */
            if (frequency > 10000u) frequency = 10000u;
            if (frequency < 80u) frequency = 80u;
            phase_step =
                ((uint64_t)frequency << 32) / NATIVE_PC_SAMPLE_RATE;

            while (sample_index < end_sample) {
                phase += phase_step;
                samples[sample_index++] =
                    (phase & UINT64_C(0x80000000)) != 0
                    ? NATIVE_PC_AMPLITUDE
                    : -NATIVE_PC_AMPLITUDE;
            }
        }
        (void)first_sample;
    }

    *sample_count = (size_t)total_samples;
    return samples;
}

void alOut(byte port, byte value)
{
    (void)port;
    (void)value;
}

void SD_Startup(void)
{
    AdLibPresent = false;
    SoundSourcePresent = false;
    SoundBlasterPresent = false;
    NeedsMusic = false;
    SoundPositioned = false;
    SoundMode = sdm_Off;
    DigiMode = sds_Off;
    MusicMode = smm_Off;
    DigiPlaying = false;
    TimeCount = 0;
    current_sound = (soundnames)-1;
    current_priority = 0;
    sound_user_hook = NULL;
}

void SD_Shutdown(void)
{
    wolf_platform_audio_clear();
    DigiPlaying = false;
    SoundMode = sdm_Off;
    MusicMode = smm_Off;
    DigiMode = sds_Off;
    current_sound = (soundnames)-1;
    current_priority = 0;
    sound_user_hook = NULL;
}

void SD_Default(boolean gotit, SDMode sound_mode, SMMode music_mode)
{
    (void)gotit;
    (void)music_mode;

    if (sound_mode == sdm_Off) {
        (void)SD_SetSoundMode(sdm_Off);
    } else {
        /* PC speaker is the native fallback; no legacy hardware probing. */
        (void)SD_SetSoundMode(sdm_PC);
    }
    (void)SD_SetMusicMode(smm_Off);
    NeedsMusic = false;
}

void SD_PositionSound(int leftvol, int rightvol)
{
    (void)leftvol;
    (void)rightvol;
}

boolean SD_PlaySound(soundnames sound)
{
    const PCSound *pc_sound;
    int queued_bytes;
    int16_t *samples;
    size_t sample_count = 0;
    boolean submitted;

    if (sound < 0 || sound >= LASTSOUND || SoundMode != sdm_PC) {
        return false;
    }

    pc_sound = (const PCSound *)audiosegs[STARTPCSOUNDS + (int)sound];
    /* Missing audio files are non-fatal and leave the game silent. */
    if (pc_sound == NULL || pc_sound->common.length == 0) {
        return false;
    }

    queued_bytes = wolf_platform_audio_queued_bytes();
    if (queued_bytes < 0) {
        return false;
    }
    if (queued_bytes > 0 && pc_sound->common.priority < current_priority) {
        return false;
    }

    samples = build_pc_samples(pc_sound, &sample_count);
    if (samples == NULL) {
        return false;
    }

    if (queued_bytes > 0) {
        wolf_platform_audio_clear();
    }
    submitted = wolf_platform_audio_submit(samples, sample_count);
    free(samples);

    if (submitted) {
        current_sound = sound;
        current_priority = pc_sound->common.priority;
    }
    return false;
}

void SD_SetPosition(int leftvol, int rightvol)
{
    (void)leftvol;
    (void)rightvol;
}

void SD_StopSound(void)
{
    wolf_platform_audio_clear();
    current_sound = (soundnames)-1;
    current_priority = 0;
}

void SD_WaitSoundDone(void)
{
    while (wolf_platform_audio_queued_bytes() > 0) {
        wolf_platform_delay_ms(1);
    }
    current_sound = (soundnames)-1;
    current_priority = 0;
}

void SD_StartMusic(MusicGroup far *music)
{
    (void)music;
}

void SD_MusicOn(void) {}
void SD_MusicOff(void) { }
void SD_FadeOutMusic(void) { }

void SD_SetUserHook(void (*hook)(void))
{
    sound_user_hook = hook;
}

boolean SD_MusicPlaying(void)
{
    return false;
}

boolean SD_SetSoundMode(SDMode mode)
{
    SD_StopSound();

    switch (mode) {
        case sdm_Off:
            SoundMode = sdm_Off;
            break;
        case sdm_PC:
        case sdm_AdLib:
            /* The native implementation provides the PC speaker voice only. */
            SoundMode = mode == sdm_Off ? sdm_Off : sdm_PC;
            break;
        default:
            return false;
    }

    return true;
}

boolean SD_SetMusicMode(SMMode mode)
{
    (void)mode;
    SD_MusicOff();
    MusicMode = smm_Off;
    NeedsMusic = false;
    return true;
}

word SD_SoundPlaying(void)
{
    int queued_bytes = wolf_platform_audio_queued_bytes();

    if (queued_bytes <= 0) {
        current_sound = (soundnames)-1;
        current_priority = 0;
        return 0;
    }
    return current_sound >= 0 ? (word)current_sound : 0;
}

void SD_SetDigiDevice(SDSMode mode)
{
    (void)mode;
    DigiMode = sds_Off;
    DigiPlaying = false;
}

void SD_PlayDigitized(word which, int leftpos, int rightpos)
{
    (void)which;
    (void)leftpos;
    (void)rightpos;
    DigiPlaying = false;
}

void SD_StopDigitized(void)
{
    DigiPlaying = false;
}

void SD_Poll(void)
{
    ++TimeCount;

    if (wolf_platform_audio_queued_bytes() == 0) {
        current_sound = (soundnames)-1;
        current_priority = 0;
    }

    if (sound_user_hook != NULL) {
        sound_user_hook();
    }
}
