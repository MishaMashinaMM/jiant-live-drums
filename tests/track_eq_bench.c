/* SPDX-License-Identifier: GPL-3.0-only
 * Host-side relative measurement only. Not an FM-1 cycle budget. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "../firmware/src/track_eq_dsp.h"

#define TRACKS 6u
#define FRAMES 64u
#define BLOCKS 1500u
static eq_dsp_t state[TRACKS];
static int32_t audio[TRACKS][EQ_STREAMS][FRAMES];
static volatile uint32_t checksum;

static uint64_t nanos(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}

static double measure(unsigned streams)
{
    const int8_t gains[3] = {15, -12, 15};
    uint64_t start;
    unsigned t, b, k, s, i;
    for (t = 0; t < TRACKS; t++) {
        eq_dsp_init(&state[t]);
        for (s = 0; s < EQ_STREAMS; s++)
            for (i = 0; i < FRAMES; i++) audio[t][s][i] = (i & 1u ? -2000 : 2000) + i;
        for (b = 0; b < 15u; b++) eq_dsp_tick(&state[t], gains, 1);
    }
    start = nanos();
    for (b = 0; b < BLOCKS; b++) {
        for (t = 0; t < TRACKS; t++) {
            eq_dsp_tick(&state[t], gains, 1);
            for (s = 0; s < streams; s++) eq_dsp_process(&state[t], s, audio[t][s], FRAMES);
        }
    }
    for (t = 0; t < TRACKS; t++)
        for (s = 0; s < streams; s++)
            for (k = 0; k < FRAMES; k++) checksum += (uint32_t)audio[t][s][k];
    return (double)(nanos() - start) / BLOCKS / 1000.0;
}

int main(void)
{
    double dry = measure(2u);
    double all = measure(5u);
    printf("EQ host %u tracks x %u samples, 3 active bands: 2 dry streams %.2f us/block; all 5 streams %.2f us/block (checksum %u)\n",
           TRACKS, FRAMES, dry, all, checksum);
    return 0;
}
