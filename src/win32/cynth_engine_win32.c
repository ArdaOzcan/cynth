/* Windows audio backend.
 *
 * Stub implementation of the internal backend interface
 * (see src/cynth_backend.h). It exists so the library compiles and links on
 * Windows; every entry point fails gracefully until a real backend lands.
 *
 * TODO: implement playback via WinMM waveOut and link winmm.
 */

#include "cynth_backend.h"
#include "clog.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct CynthEngine
{
    int placeholder; /* TODO: replace with WinMM waveOut handles */
};

CynthEngine*
cynth_engine_init(CynthSampleSpec* ss, const char* device)
{
    (void)ss;
    (void)device;
    CLOG_ERROR("Cynth Windows audio backend is not implemented yet.");
    return NULL;
}

uint32_t
cynth_engine_get_period_size(CynthEngine* engine)
{
    (void)engine;
    return 0;
}

CynthError
cynth_engine_write_buffer(CynthEngine* engine,
                          const CynthBuffer* buffer,
                          size_t frame_amount)
{
    (void)engine;
    (void)buffer;
    (void)frame_amount;
    return CYNTH_ERROR_WRITE;
}

void
cynth_engine_deinit(CynthEngine* engine)
{
    free(engine);
}

CynthError
cynth_engine_drain(CynthEngine* engine)
{
    (void)engine;
    return CYNTH_ERROR_DRAIN;
}

CynthError
cynth_engine_drop(CynthEngine* engine)
{
    (void)engine;
    /* No CYNTH_ERROR_DROP variant exists; WRITE is the closest failure. */
    return CYNTH_ERROR_WRITE;
}
