#ifndef CYNTH_COMMON
#define CYNTH_COMMON

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    CYNTH_SAMPLE_U8,
    /**< Unsigned 8 Bit PCM */

    CYNTH_SAMPLE_ALAW,
    /**< 8 Bit a-Law */

    CYNTH_SAMPLE_ULAW,
    /**< 8 Bit mu-Law */

    CYNTH_SAMPLE_S16LE,
    /**< Signed 16 Bit PCM, little endian (PC) */

    CYNTH_SAMPLE_S16BE,
    /**< Signed 16 Bit PCM, big endian */

    CYNTH_SAMPLE_FLOAT32LE,
    /**< 32 Bit IEEE floating point, little endian (PC), range -1.0 to 1.0 */

    CYNTH_SAMPLE_FLOAT32BE,
    /**< 32 Bit IEEE floating point, big endian, range -1.0 to 1.0 */

    CYNTH_SAMPLE_S32LE,
    /**< Signed 32 Bit PCM, little endian (PC) */

    CYNTH_SAMPLE_S32BE,
    /**< Signed 32 Bit PCM, big endian */

    CYNTH_SAMPLE_S24LE,
    /**< Signed 24 Bit PCM packed, little endian (PC). \since 0.9.15 */

    CYNTH_SAMPLE_S24BE,
    /**< Signed 24 Bit PCM packed, big endian. \since 0.9.15 */

    CYNTH_SAMPLE_S24_32LE,
    /**< Signed 24 Bit PCM in LSB of 32 Bit words, little endian (PC). \since
       0.9.15 */

    CYNTH_SAMPLE_S24_32BE,
    /**< Signed 24 Bit PCM in LSB of 32 Bit words, big endian. \since 0.9.15 */

    CYNTH_SAMPLE_MAX,
    /**< Upper limit of valid sample types */

    CYNTH_SAMPLE_INVALID = -1
    /**< An invalid value */
} CynthSampleFormat;

typedef struct
{
    CynthSampleFormat format;
    uint32_t rate;
    uint8_t channels;
} CynthSampleSpec;

typedef struct
{
    int16_t* data;
    size_t frame_amount;
    CynthSampleSpec ss;
} CynthBuffer;

typedef struct
{
    float attack;
    float decay;
    float sustain;
    float release;
} CynthEnvelope;

typedef struct
{
    float frequency;
    float start_time;
    float duration;
    float volume;
} CynthNote;

typedef enum CynthError
{
    CYNTH_ERROR_NONE = 0,
    CYNTH_ERROR_WRITE,
    CYNTH_ERROR_DRAIN,
} CynthError;

typedef enum
{
    CYNTH_MIDI_FORMAT_SINGLE = 0,
    CYNTH_MIDI_FORMAT_VERTICAL = 1,
    CYNTH_MIDI_FORMAT_HORIZONTAL = 2,
} CynthMIDIFormat;

typedef struct
{
    CynthMIDIFormat format;
    uint16_t num_tracks;
    uint16_t division;
} CynthMIDIHeader;

typedef struct
{
    uint8_t key;
    uint8_t velocity;
} CynthMIDIEvent_Note;

typedef enum
{
    CYNTH_MIDI_EVENT_NOTE_ON,
    CYNTH_MIDI_EVENT_NOTE_OFF,
} CynthMIDIEventType;

typedef struct
{
    CynthMIDIEventType type;
    uint8_t delta_time;
    union
    {
        CynthMIDIEvent_Note note;
    };
} CynthMIDIEvent;

typedef struct
{
    size_t event_amount;
    CynthMIDIEvent* events;
} CynthMIDITrackInfo;

typedef struct
{
    CynthMIDIHeader header;
    CynthMIDITrackInfo* tracks;
} CynthMIDIObject;

#endif
