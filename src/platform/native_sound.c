/*
 * Native sound manager.
 *
 * PC-speaker effects are packed resource streams: a four-byte command count,
 * two-byte priority, then one frequency command per 140 Hz timer tick. Decode
 * those bytes explicitly because a native compiler may pad the old C struct.
 * The commands are synthesised to mono PCM and queued through SDL3.
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
#define NATIVE_PC_SOUND_HEADER_SIZE 6u

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
WolfNativeTick WolfNativeTimeCount;
static WolfNativeTick game_clock_enabled;

static void (*sound_user_hook)(void);
static soundnames current_sound;
static word current_priority;

static uint16_t read_u16_le(const byte *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_u32_le(const byte *data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static int16_t *build_pc_samples(
    const byte *commands,
    uint32_t command_count,
    size_t *sample_count)
{
    uint64_t total_samples;
    uint64_t phase = 0;
    uint64_t sample_index;
    uint32_t tick;
    int16_t *samples;

    if (commands == NULL || sample_count == NULL ||
        command_count == 0 ||
        command_count > NATIVE_PC_MAX_DURATION_TICKS) {
        return NULL;
    }

    total_samples = ((uint64_t)command_count *
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
    for (tick = 0; tick < command_count; ++tick) {
        unsigned command = commands[tick];
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
    WOLF_SET_TIMECNT(0);
    wolf_native_tick_store(&game_clock_enabled, 1u);
    current_sound = (soundnames)-1;
    current_priority = 0;
    sound_user_hook = NULL;
}

void SD_Shutdown(void)
{
    wolf_native_tick_store(&game_clock_enabled, 0u);
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
    const byte *chunk;
    uint32_t command_count;
    uint16_t priority;
    int queued_bytes;
    int16_t *samples;
    size_t sample_count = 0;
    boolean submitted;

    if (sound < 0 || sound >= LASTSOUND || SoundMode != sdm_PC) {
        return false;
    }

    chunk = (const byte *)audiosegs[STARTPCSOUNDS + (int)sound];
    if (chunk == NULL) {
        return false;
    }

    command_count = read_u32_le(chunk);
    priority = read_u16_le(chunk + 4);
    if (command_count == 0) {
        return false;
    }

    queued_bytes = wolf_platform_audio_queued_bytes();
    if (queued_bytes < 0) {
        return false;
    }
    if (queued_bytes > 0 && priority < current_priority) {
        return false;
    }

    samples = build_pc_samples(
        chunk + NATIVE_PC_SOUND_HEADER_SIZE, command_count, &sample_count);
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
        current_priority = priority;
    }
    return submitted;
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
void SD_MusicOff(void) {}
void SD_FadeOutMusic(void) {}

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
            SoundMode = sdm_PC;
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

void SD_TimerTick(void)
{
    if (wolf_native_tick_load(&game_clock_enabled) != 0) {
        (void)wolf_native_tick_increment(&WolfNativeTimeCount);
    }
}

void SD_Poll(void)
{
    if (wolf_platform_audio_queued_bytes() == 0) {
        current_sound = (soundnames)-1;
        current_priority = 0;
    }

    if (sound_user_hook != NULL) {
        sound_user_hook();
    }
}
