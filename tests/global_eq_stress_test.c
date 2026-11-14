/* Stress the global EQ under sustained seven-bus traffic and rapid control changes.
 * Host-only correctness/sanitizer test; NOT a target CPU benchmark. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/track_eq_dsp.h"

#define BLOCK 128u
#define BLOCKS 4096u

int main(void)
{
    eq_dsp_t eq;
    int32_t samples[EQ_STREAMS][BLOCK];
    uint32_t seed = 0x12345678u;
    uint64_t checksum = 0;
    unsigned block, stream, i;
    eq_dsp_init(&eq);
    for (block = 0; block < BLOCKS; ++block) {
        int8_t gain[EQ_BANDS] = {
            (int8_t)((block / 11u) % 31u - 15),
            (int8_t)((block / 17u) % 31u - 15),
            (int8_t)((block / 23u) % 31u - 15)
        };
        unsigned on = (block % 97u) >= 5u;
        eq_dsp_tick(&eq, gain, (uint8_t)on);
        for (stream = 0; stream < EQ_STREAMS; ++stream) {
            for (i = 0; i < BLOCK; ++i) {
                seed ^= seed << 13;
                seed ^= seed >> 17;
                seed ^= seed << 5;
                samples[stream][i] = (int32_t)(seed % 600001u) - 300000;
            }
            eq_dsp_process(&eq, stream, samples[stream], BLOCK);
            for (i = 0; i < BLOCK; ++i) {
                int64_t v = samples[stream][i];
                assert(v > -100000000LL && v < 100000000LL);
                checksum += (uint32_t)v;
            }
        }
    }
    printf("PASS 4096 blocks x 7 streams, gain/bypass stress; checksum %llu\n",
           (unsigned long long)checksum);
    return 0;
}
