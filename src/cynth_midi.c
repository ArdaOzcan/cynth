#include "ccore.h"
#include "clog.h"
#include "cynth.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const uint8_t* data;
    size_t offset;
    size_t size;
} ByteReader;

typedef enum
{
    CYNTH_MIDI_CHUNK_TYPE_NONE = -1,
    CYNTH_MIDI_CHUNK_TYPE_HEADER,
    CYNTH_MIDI_CHUNK_TYPE_TRACK,
} CynthMIDIChunkType;

void
cynth_midi_object_init(CynthMIDIObject* midi, Allocator* alloc)
{
    midi->header = (CynthMIDIHeader){ 0 };
    /* Default to 90 BPM */
    midi->header.tempo = 1e6 * 0.25 * (90.0 / 60.0);
    midi->tracks = array(CynthMIDITrackInfo, 32, alloc);
}

void
cynth_midi_track_init(CynthMIDITrackInfo* track, Allocator* alloc)
{
    track->events = array(CynthMIDIEvent, 128, alloc);
}

#define byte_reader_peek(reader, T) ((T*)(&((reader)->data[(reader)->offset])))
#define byte_reader_read(reader, T)                                            \
    ((reader)->offset += sizeof(T),                                            \
     (T*)(&((reader)->data[(reader)->offset - sizeof(T)])))

/* Read variable-length quantity number. */
uint32_t
cynth_midi_read_vlq(uint32_t vn, size_t* byte_length)
{
    uint32_t result = 0;
    uint8_t* vn_bytes = (uint8_t*)&vn;
    unsigned int i = 0;
    for (i = 3; i >= 0; i--) {
        uint8_t seven_bits = vn_bytes[i] & 0x7F;
        result |= seven_bits;
        if ((vn_bytes[i] & 0x80) == 0) {
            *byte_length = 4 - i;
            return result;
        }

        result <<= 7;
    }

    *byte_length = 4 - i;
    return result;
}

/* Big-endian */
static uint32_t
to_be32(const void* ptr)
{
    const uint8_t* p = (const uint8_t*)ptr;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

/* Big-endian */
static uint16_t
to_be16(const void* ptr)
{
    const uint8_t* p = (const uint8_t*)ptr;
    return ((uint32_t)p[0] << 8) | ((uint32_t)p[1]);
}

static uint32_t
byte_reader_read_vlq_be32(ByteReader* reader)
{
    uint32_t vn = to_be32(&reader->data[reader->offset]);
    size_t length = 0;
    uint32_t val = cynth_midi_read_vlq(vn, &length);
    reader->offset += length;
    return val;
}

static void
byte_reader_init(ByteReader* reader, const void* data, size_t size)
{
    reader->data = data;
    reader->offset = 0;
    reader->size = size;
}

CynthMIDIChunkType
cynth_midi_read_chunk_type(ByteReader* reader)
{
    char type[4] = { 0 };
    unsigned int i = 0;
    for (i = 0; i < 4; i++)
        type[i] = *byte_reader_read(reader, char);

    if (strncmp(type, "MThd", 4) == 0) {
        return CYNTH_MIDI_CHUNK_TYPE_HEADER;
    } else if (strncmp(type, "MTrk", 4) == 0) {
        CLOG_DEBUG("Track");
        return CYNTH_MIDI_CHUNK_TYPE_TRACK;
    }

    return CYNTH_MIDI_CHUNK_TYPE_NONE;
}

void
cynth_midi_read_chunk_header(ByteReader* reader, CynthMIDIHeader* out_header)
{
    uint32_t length = to_be32(byte_reader_read(reader, uint32_t));
    assert(length == 6);

    uint16_t format = to_be16(byte_reader_read(reader, uint16_t));
    assert(format <= 2);

    uint16_t num_tracks = to_be16(byte_reader_read(reader, uint16_t));
    assert(format != 0 || num_tracks <= 1);
    uint16_t division = to_be16(byte_reader_read(reader, uint16_t));

    CLOG_DEBUG("%04x, %04x, %04x", format, num_tracks, division);
    out_header->format = format;
    out_header->division = division;
    out_header->num_tracks = num_tracks;
}

static void
cynth_midi_read_event_midi(ByteReader* reader,
                           CynthMIDIObject* midi,
                           uint32_t delta_time,
                           uint8_t previous_status,
                           uint8_t* new_status)
{
    CLOG_DEBUG("MIDI Event");
    uint8_t status = *byte_reader_peek(reader, uint8_t);
    if (status & 0x80) {
        status = 0xF0 & *byte_reader_read(reader, uint8_t);
    } else {
        /* Running status */
        status = previous_status;
    }

    *new_status = status;
    CynthMIDIEvent evt = { 0 };
    evt.delta_time = delta_time;
    evt.type = CYNTH_MIDI_EVENT_UNKNOWN;

    size_t track_idx = array_len(midi->tracks) - 1;

    switch (status) {
        case 0x80:
            CLOG_DEBUG("Note off");
            evt.type = CYNTH_MIDI_EVENT_NOTE_OFF;
            evt.note.key = 0x7F & *byte_reader_read(reader, uint8_t);
            evt.note.velocity = 0x7F & *byte_reader_read(reader, uint8_t);
            CLOG_DEBUG("Added evt %d to track %u. New length of track: %zu"
                       " delta time = %lu",
                       evt.type,
                       track_idx,
                       array_len(midi->tracks[track_idx].events),
                       evt.delta_time);
            break;
        case 0x90:
            CLOG_DEBUG("Note on");
            evt.type = CYNTH_MIDI_EVENT_NOTE_ON;
            evt.note.key = 0x7F & *byte_reader_read(reader, uint8_t);
            evt.note.velocity = 0x7F & *byte_reader_read(reader, uint8_t);
            CLOG_DEBUG("Added evt %d to track %d. New length of track: %zu"
                       " delta time = %lu",
                       evt.type,
                       track_idx,
                       array_len(midi->tracks[track_idx].events),
                       evt.delta_time);
            break;
        case 0xa0:
            CLOG_DEBUG("Polyphonic key pressure");
            (void)*byte_reader_read(reader, uint8_t);
            (void)*byte_reader_read(reader, uint8_t);
            break;
        case 0xb0:
            CLOG_DEBUG("Control change");
            (void)*byte_reader_read(reader, uint8_t);
            (void)*byte_reader_read(reader, uint8_t);
            break;
        case 0xc0:
            CLOG_DEBUG("Program change");
            (void)*byte_reader_read(reader, uint8_t);
            break;
        case 0xd0:
            CLOG_DEBUG("Channel pressure");
            (void)*byte_reader_read(reader, uint8_t);
            break;
        case 0xe0:
            CLOG_DEBUG("Pitch wheel change");
            (void)*byte_reader_read(reader, uint8_t);
            (void)*byte_reader_read(reader, uint8_t);
            break;
        default:
            CLOG_WARNING("Not implemented");
    }
    array_append(midi->tracks[track_idx].events, evt);
}

void
cynth_midi_read_event_sysex(ByteReader* reader)
{
    assert(*byte_reader_read(reader, uint8_t) == 0xF0);
    uint32_t length = byte_reader_read_vlq_be32(reader);
    uint32_t i = 0;
    for (i = 0; i < length; i++)
        (void)*byte_reader_read(reader, uint8_t);
    CLOG_WARNING("Sysex message ignored.");
}

void
cynth_midi_read_event_meta(ByteReader* reader, CynthMIDIHeader* header)
{
    (void)*byte_reader_read(reader, uint8_t); /* Status byte */

    uint8_t type = *byte_reader_read(reader, uint8_t);
    uint32_t length = byte_reader_read_vlq_be32(reader);
    CLOG_DEBUG("Event length: %u", length);

    switch (type) {
        case 0x00:
            if (length == 2) {
                uint16_t seq = (*byte_reader_read(reader, uint8_t) << 8) |
                               *byte_reader_read(reader, uint8_t);
                CLOG_DEBUG("Sequence Number: %u", seq);
            }
            break;

        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07: {
            char* text = malloc(length + 1);
            for (uint32_t i = 0; i < length; i++)
                text[i] = *byte_reader_read(reader, uint8_t);
            text[length] = '\0';
            CLOG_DEBUG("Text Meta Event 0x%02X: %s", type, text);
            free(text);
        } break;

        case 0x20:
            if (length == 1) {
                uint8_t chan = *byte_reader_read(reader, uint8_t);
                CLOG_DEBUG("MIDI Channel Prefix: %u", chan);
            }
            break;

        case 0x2F:
            if (length == 0) {
                CLOG_DEBUG("End of Track");
            }
            break;

        case 0x51:
            if (length == 3) {
                uint32_t tempo = (*byte_reader_read(reader, uint8_t) << 16) |
                                 (*byte_reader_read(reader, uint8_t) << 8) |
                                 *byte_reader_read(reader, uint8_t);
                CLOG_DEBUG("Set Tempo: %u microseconds per quarter note",
                           tempo);
                header->tempo = tempo;
            }
            break;

        case 0x54:
            if (length == 5) {
                uint8_t hr = *byte_reader_read(reader, uint8_t);
                uint8_t mn = *byte_reader_read(reader, uint8_t);
                uint8_t se = *byte_reader_read(reader, uint8_t);
                uint8_t fr = *byte_reader_read(reader, uint8_t);
                uint8_t ff = *byte_reader_read(reader, uint8_t);
                CLOG_DEBUG(
                  "SMPTE Offset: %02u:%02u:%02u:%02u.%02u", hr, mn, se, fr, ff);
            }
            break;

        case 0x58:
            if (length == 4) {
                uint8_t nn = *byte_reader_read(reader, uint8_t);
                uint8_t dd = *byte_reader_read(reader, uint8_t);
                uint8_t cc = *byte_reader_read(reader, uint8_t);
                uint8_t bb = *byte_reader_read(reader, uint8_t);
                CLOG_DEBUG("Time Signature: %u/%u, MIDI clocks per click: %u, "
                           "32nd notes per quarter: %u",
                           nn,
                           1 << dd,
                           cc,
                           bb);
            }
            break;

        case 0x59:
            if (length == 2) {
                int8_t sf = (int8_t)*byte_reader_read(reader, uint8_t);
                uint8_t mi = *byte_reader_read(reader, uint8_t);
                CLOG_DEBUG("Key Signature: %d %s", sf, mi ? "minor" : "major");
            }
            break;

        case 0x7F: {
            CLOG_DEBUG("Sequencer Specific Event, length %u", length);
            uint32_t i = 0;
            for (i = 0; i < length; i++)
                (void)*byte_reader_read(reader, uint8_t);
        } break;

        default: {
            CLOG_WARNING("Unknown Meta Event: 0x%02X, length %u", type, length);
            uint32_t i = 0;
            for (i = 0; i < length; i++)
                (void)*byte_reader_read(reader, uint8_t);
            break;
        }
    }
}

static void
cynth_midi_read_event(ByteReader* reader,
                      CynthMIDIObject* midi,
                      uint8_t previous_midi_evt,
                      uint8_t* new_status)
{
    uint32_t delta_time = byte_reader_read_vlq_be32(reader);
    CLOG_DEBUG("Delta time: %u", delta_time);

    uint8_t status = *byte_reader_peek(reader, uint8_t);
    CLOG_DEBUG("Status: %u", status);
    if (0xF0 <= status && status <= 0xFE) {
        cynth_midi_read_event_sysex(reader);
    } else if (status == 0xFF) {
        cynth_midi_read_event_meta(reader, &midi->header);
    } else {
        cynth_midi_read_event_midi(
          reader, midi, delta_time, previous_midi_evt, new_status);
    }
}

static void
cynth_midi_read_chunk_track(ByteReader* reader, CynthMIDIObject* midi)
{
    uint32_t length = to_be32(byte_reader_read(reader, uint32_t));
    CLOG_DEBUG("Track Chunk length: %u", length);
    size_t start_offset = reader->offset;
    uint8_t previous_midi_evt = 0;
    while (reader->offset - start_offset < length) {
        cynth_midi_read_event(
          reader, midi, previous_midi_evt, &previous_midi_evt);
    }
}

static void* (*original_alloc)(size_t, void*);
static void* (*original_realloc)(void*, size_t, size_t, void*);

void*
my_alloc(size_t size, void* ctx)
{
    const VArena* varena = (VArena*)ctx;
    void* ptr = original_alloc(size, ctx);
    CLOG_DEBUG("Alloc with size %zu. Returned %p -> %p", size, ptr, ptr + size);
    return ptr;
}

void*
my_realloc(void* start, size_t old_size, size_t new_size, void* context)
{
    const VArena* varena = (VArena*)context;
    void* result = original_realloc(start, old_size, new_size, context);
    CLOG_DEBUG(
      "ReAlloc from %p -> %p (%zu bytes). Returning %p -> %p (%zu bytes)",
      start,
      start + old_size,
      old_size,
      result,
      result + new_size,
      new_size);
    return result;
}

void
cynth_midi_import(const void* file_data,
                  size_t file_size,
                  CynthMIDIObject* out_midi)
{
    VArena varena = { 0 };
    varena_init(&varena, 1024 * MEGABYTE);
    Allocator alloc = varena_allocator(&varena);
    original_alloc = alloc.alloc;
    original_realloc = alloc.realloc;
    alloc.alloc = my_alloc;
    alloc.realloc = my_realloc;

    CynthMIDIObject midi = { 0 };
    cynth_midi_object_init(&midi, &alloc);

    ByteReader reader = { 0 };
    byte_reader_init(&reader, file_data, file_size);

    /* First chunk should be header */
    if (cynth_midi_read_chunk_type(&reader) != CYNTH_MIDI_CHUNK_TYPE_HEADER) {
        CLOG_ERROR("First chunk should be header.");
        return;
    }

    cynth_midi_read_chunk_header(&reader, &midi.header);

    size_t i = 0;
    for (; i < midi.header.num_tracks; i++) {
        CynthMIDITrackInfo track = { 0 };
        cynth_midi_track_init(&track, &alloc);
        array_append(midi.tracks, track);

        if (cynth_midi_read_chunk_type(&reader) !=
            CYNTH_MIDI_CHUNK_TYPE_TRACK) {
            CLOG_ERROR("Chunk should be track.");
            return;
        }
        cynth_midi_read_chunk_track(&reader, &midi);
        midi.tracks[i].event_amount = array_len(midi.tracks[i].events);
    }

    *out_midi = midi;
}

double
cynth_midi_time_to_seconds(CynthMIDIHeader header, uint32_t delta_time)
{
    if (header.division & 0x8000) {
        /* Negative SMPT */
        int8_t smpte = (int8_t)(header.division & 0x7F00 >> 8);
        uint8_t ticks_per_frame = header.division & 0x00FF;

        float coeff = 0.0f;
        switch (smpte) {
            case -29:
                coeff = 29.97;
                break;
            case -24:
            case -25:
            case -30:
                coeff = (float)(-smpte);
                break;
            default:
                CLOG_ERROR(
                  "MIDI time could not be calculated: Negative SMPTE should be "
                  "one of these values: -24, -25, -29, -30. It was %d.",
                  smpte);
                return 0.0;
        }

        return coeff * delta_time;
        /* CLOG_INFO(); */
    } else {
        /* Ticks per quarter note */
        uint16_t ticks_per_quarter_note = header.division & 0x7FFF;
        /* CLOG_INFO("Division: Ticks per quarter note: %u",
                  ticks_per_quarter_note); */
        if (ticks_per_quarter_note == 0) {
            CLOG_WARNING("Ticks per quarter note is zero.");
            return 0;
        }

        double seconds_per_quarter = (double)header.tempo / 1e6;
        double quarter_notes = (double)delta_time / ticks_per_quarter_note;
        return seconds_per_quarter * quarter_notes;
    }
}
