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

typedef struct TestPCSound {
    SoundCommon common;
    byte data[4];
} TestPCSound;

static int16_t captured_samples[4096];
static size_t captured_sample_count;
static int submit_calls;
static int clear_calls;
static int fake_queued_bytes;
static boolean fake_audio_available = true;

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

static void test_pc_speaker_stream_timing_and_silence(void)
{
    TestPCSound sound;

    memset(&sound, 0, sizeof(sound));
    sound.common.length = 4;
    sound.common.priority = 10;
    sound.data[0] = 20;
    sound.data[1] = 0;
    sound.data[2] = 10;
    sound.data[3] = 10;
    audiosegs[STARTPCSOUNDS + SELECTWPNSND] = (byte *)&sound;

    SD_Startup();
    assert(SD_SetSoundMode(sdm_PC));
    assert(SD_PlaySound(SELECTWPNSND) == false);

    /* Four 140 Hz commands at 22050 Hz produce exactly 630 output frames. */
    assert(captured_sample_count == 630);
    assert(captured_samples[0] != 0);

    /* The second command is the PC speaker's explicit silence marker. */
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

    memset(&low_priority, 0, sizeof(low_priority));
    low_priority.common.length = 2;
    low_priority.common.priority = 1;
    low_priority.data[0] = 10;
    low_priority.data[1] = 10;
    audiosegs[STARTPCSOUNDS + HITWALLSND] = (byte *)&low_priority;

    memset(&high_priority, 0, sizeof(high_priority));
    high_priority.common.length = 2;
    high_priority.common.priority = 30;
    high_priority.data[0] = 8;
    high_priority.data[1] = 8;
    audiosegs[STARTPCSOUNDS + SELECTITEMSND] = (byte *)&high_priority;

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
