#ifdef NDEBUG
#undef NDEBUG
#endif

#include "WL_DEF.H"
#include "platform.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

byte *audiosegs[NUMSNDCHUNKS];

#define TEST_PC_SOUND_HEADER_SIZE 6u

typedef struct TestPCSound {
    byte bytes[TEST_PC_SOUND_HEADER_SIZE + 4u];
} TestPCSound;

static int16_t captured_samples[4096];
static size_t captured_sample_count;
static int submit_calls;
static int clear_calls;
static int fake_queued_bytes;
static boolean fake_audio_available = true;

static void write_u16_le(byte *data, uint16_t value)
{
    data[0] = (byte)(value & 0xffu);
    data[1] = (byte)(value >> 8);
}

static void write_u32_le(byte *data, uint32_t value)
{
    data[0] = (byte)(value & 0xffu);
    data[1] = (byte)((value >> 8) & 0xffu);
    data[2] = (byte)((value >> 16) & 0xffu);
    data[3] = (byte)(value >> 24);
}

bool wolf_platform_audio_submit(const int16_t *samples, size_t sample_count)
{
    assert(samples != NULL || sample_count == 0);
    assert(sample_count <= sizeof(captured_samples) / sizeof(captured_samples[0]));
    if (!fake_audio_available) {
        return false;
    }
    memcpy(captured_samples, samples, sample_count * sizeof(*samples));
    captured_sample_count = sample_count;
    ++submit_calls;
    fake_queued_bytes = (int)(sample_count * sizeof(*samples));
    return true;
}

void wolf_platform_audio_clear(void)
{
    ++clear_calls;
    fake_queued_bytes = 0;
}

int wolf_platform_audio_queued_bytes(void)
{
    return fake_audio_available ? fake_queued_bytes : -1;
}

void wolf_platform_delay_ms(uint32_t milliseconds)
{
    (void)milliseconds;
}

static void setup_pc_sound(TestPCSound *sound,
    uint32_t length, uint16_t priority,
    byte a, byte b, byte c, byte d)
{
    memset(sound, 0, sizeof(*sound));
    write_u32_le(sound->bytes, length);
    write_u16_le(sound->bytes + 4, priority);
    sound->bytes[TEST_PC_SOUND_HEADER_SIZE + 0] = a;
    sound->bytes[TEST_PC_SOUND_HEADER_SIZE + 1] = b;
    sound->bytes[TEST_PC_SOUND_HEADER_SIZE + 2] = c;
    sound->bytes[TEST_PC_SOUND_HEADER_SIZE + 3] = d;
}

static void test_pc_speaker_stream_timing_and_silence(void)
{
    TestPCSound sound;

    setup_pc_sound(&sound, 4, 10, 20, 0, 10, 10);
    audiosegs[STARTPCSOUNDS + SELECTWPNSND] = sound.bytes;

    SD_Startup();
    assert(SD_SetSoundMode(sdm_PC));
    assert(SD_PlaySound(SELECTWPNSND) == false);
    assert(captured_sample_count == 630);
    assert(captured_samples[0] != 0);

    {
        size_t index;
        for (index = 157; index < 315; ++index) {
            assert(captured_samples[index] == 0);
        }
    }

    assert(captured_samples[315] != 0);
    assert(SD_SoundPlaying() == (word)SELECTWPNSND);
}

static void test_priority_and_replacement(void)
{
    TestPCSound low_priority;
    TestPCSound high_priority;

    setup_pc_sound(&low_priority, 2, 1, 10, 10, 0, 0);
    audiosegs[STARTPCSOUNDS + HITWALLSND] = low_priority.bytes;

    setup_pc_sound(&high_priority, 2, 30, 8, 8, 0, 0);
    audiosegs[STARTPCSOUNDS + SELECTITEMSND] = high_priority.bytes;

    {
        int previous_submits = submit_calls;
        int previous_clears = clear_calls;
        assert(SD_PlaySound(HITWALLSND) == false);
        assert(submit_calls == previous_submits);
        assert(clear_calls == previous_clears);
    }

    {
        int previous_clears = clear_calls;
        assert(SD_PlaySound(SELECTITEMSND) == false);
        assert(clear_calls == previous_clears + 1);
        assert(SD_SoundPlaying() == (word)SELECTITEMSND);
    }

    SD_StopSound();
    assert(fake_queued_bytes == 0);
    assert(SD_SoundPlaying() == 0);
}

static void test_missing_audio_device_and_chunk(void)
{
    fake_audio_available = false;
    assert(SD_PlaySound(SELECTWPNSND) == false);

    fake_audio_available = true;
    fake_queued_bytes = 0;
    audiosegs[STARTPCSOUNDS + HEARTBEATSND] = NULL;
    assert(SD_PlaySound(HEARTBEATSND) == false);
}

int main(void)
{
    test_pc_speaker_stream_timing_and_silence();
    test_priority_and_replacement();
    test_missing_audio_device_and_chunk();
    SD_Shutdown();
    puts("native sound tests passed");
    return 0;
}
