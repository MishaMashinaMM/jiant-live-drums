/* Host verification of JLD's standalone, fixed-point global EQ. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/track_eq_dsp.h"
#define FS 44117.64705882353
#define N 32768u
#define BLK 128u
#define AMP 262144.0
#define PI 3.14159265358979323846

static double run_freq(int band, int db, double hz)
{
    eq_dsp_t s;
    int32_t b[BLK];
    int8_t gains[3] = {0, 0, 0};
    double out = 0.0, in = 0.0;
    unsigned i, j;
    gains[band] = (int8_t)db;
    eq_dsp_init(&s);
    for (i = 0; i < 32; i++) eq_dsp_tick(&s, gains, 1);
    for (i = 0; i < N; i += BLK) {
        for (j = 0; j < BLK; j++) b[j] = (int32_t)lrint(AMP * sin(2.0 * PI * hz * (i+j) / FS));
        eq_dsp_process(&s, 0, b, BLK);
        if (i >= N/2) for (j = 0; j < BLK; j++) {
            out += (double)b[j] * b[j];
            double x = AMP * sin(2.0 * PI * hz * (i+j) / FS);
            in += x*x;
        }
    }
    return 10.0 * log10(out/in);
}
static void check_close(const char *name, double measured, double expected, double tol)
{
    if (!isfinite(measured) || fabs(measured - expected) > tol) {
        fprintf(stderr,"FAIL %s: %.3f dB expected %.3f +/- %.3f\n",name,measured,expected,tol);
        exit(1);
    }
    printf("PASS %s: %.2f dB\n",name,measured);
}
/* Compare the hardware-compatible LOW's Q28 fractional feedback against the
 * same coefficients evaluated with a full double-precision recursive state.
 * The old LOW rounded away the recursive fraction every sample, accumulating
 * thousands of sample units of error in repeated note/silence sequences. */
static void check_low_fractional_feedback(int gain)
{
    static const int32_t coef[2][5] = {
        {265734488, -527533773, 261804926, -527520735, 259116996}, /* -15 */
        {271163877, -532882541, 261750703, -532895712, 264465953}  /* +15 */
    };
    const int32_t *c = coef[gain < 0 ? 0 : 1];
    eq_dsp_t s;
    int32_t b[BLK];
    int8_t gains[3] = {(int8_t)gain, 0, 0};
    double x1=0, x2=0, y1=0, y2=0, worst=0;
    unsigned k, j;
    eq_dsp_init(&s);
    for (k=0; k<20; k++) eq_dsp_tick(&s,gains,1);
    for (k=0; k<22000; k+=BLK) {
        for (j=0; j<BLK; j++) {
            unsigned step = (k+j)%2200u;
            double envelope = step >= 1500u ? 0.0 :
                  fmin(1.0, fmin((double)step/100.0, (1500.0-(double)step)/100.0));
            b[j] = (int32_t)lrint(600000.0*envelope*
                        sin(2.0*PI*110.0*(k+j)/FS));
        }
        /* Keep an exact copy for the independent reference. */
        int32_t input[BLK];
        memcpy(input,b,sizeof b);
        eq_dsp_process(&s,0,b,BLK);
        for (j=0; j<BLK; j++) {
            double x = input[j];
            double y = ((double)c[0]*x + (double)c[1]*x1 +
                        (double)c[2]*x2 - (double)c[3]*y1 -
                        (double)c[4]*y2) / 268435456.0;
            double err = fabs((double)b[j]-y);
            if (err > worst) worst=err;
            x2=x1; x1=x; y2=y1; y1=y;
        }
    }
    if (worst > 2.0) {
        fprintf(stderr,"FAIL LOW fractional reference at %+d: %.2f sample units\n",gain,worst);
        exit(1);
    }
    printf("PASS LOW %+d fractional reference: max error %.3f units\n",gain,worst);
}

int main(void)
{
    eq_dsp_t s;
    int32_t a[BLK], original[BLK];
    int8_t gains[3] = {0, 0, 0};
    unsigned i;
    eq_dsp_init(&s);
    for (i = 0; i < BLK; i++) original[i] = a[i] = (int32_t)((i * 17011u) % 345678u) - 150000;
    eq_dsp_tick(&s,gains,1);
    eq_dsp_process(&s,0,a,BLK);
    assert(!memcmp(a,original,sizeof a));
    puts("PASS flat EQ bit-exact");
    /* Broader shelves keep the nominal 50/10000 Hz corner frequencies while
     * also shaping musical bass and presence frequencies. */
    check_close("LOW +15 at 50 Hz",run_freq(0,15,50),7.5,0.6);
    check_close("LOW -15 at 50 Hz",run_freq(0,-15,50),-7.5,0.6);
    check_close("LOW +15 at 100 Hz",run_freq(0,15,100),5.2,0.6);
    check_close("LOW -15 at 100 Hz",run_freq(0,-15,100),-5.2,0.6);
    check_close("LOW +15 at 200 Hz",run_freq(0,15,200),2.7,0.5);
    check_close("MID +15 at 700 Hz",run_freq(1,15,700),15,0.8);
    check_close("MID -15 at 700 Hz",run_freq(1,-15,700),-15,0.8);
    check_close("HIGH +15 at 10 kHz",run_freq(2,15,10000),7.5,0.5);
    check_close("HIGH -15 at 10 kHz",run_freq(2,-15,10000),-7.5,0.5);
    check_close("HIGH +15 at 4 kHz",run_freq(2,15,4000),3.8,0.5);
    check_close("HIGH -15 at 4 kHz",run_freq(2,-15,4000),-3.8,0.5);
    check_close("LOW rejects 1 kHz",run_freq(0,15,1000),0,0.3);
    check_close("HIGH rejects 100 Hz",run_freq(2,15,100),0,0.3);
    eq_dsp_init(&s);
    gains[0]=15; gains[1]=-15; gains[2]=15;
    for (i=0;i<40;i++) eq_dsp_tick(&s,gains,1);
    assert(s.gain[0]==15 && s.gain[1]==-15 && s.gain[2]==15);
    for (i=0;i<10000;i+=BLK) {
        unsigned j;
        for (j=0;j<BLK;j++) a[j]=(int32_t)(1000000*sin(i+j));
        eq_dsp_process(&s,0,a,BLK);
    }
    puts("PASS repeated processing and gain slew");
    /* One global instance processes seven shared buses, not six track instances. */
    eq_dsp_init(&s);
    gains[0] = 15; gains[1] = 15; gains[2] = -15;
    for (i = 0; i < 20; i++) eq_dsp_tick(&s, gains, 1);
    for (unsigned stream = 0; stream < EQ_STREAMS; stream++) {
        for (i = 0; i < BLK; i++) a[i] = original[i];
        eq_dsp_process(&s, stream, a, BLK);
        assert(memcmp(a, original, sizeof a) != 0);
    }
    assert(EQ_STREAMS == 7u);
    puts("PASS seven global dry/send/slicer streams");

    /* A gain transition from positive through zero to negative must preserve
     * a neutral block so histories are refreshed before cut resumes. */
    eq_dsp_init(&s);
    gains[0] = 1; gains[1] = gains[2] = 0;
    eq_dsp_tick(&s, gains, 1);
    assert(s.gain[0] == 1);
    s.was_active = 1; /* mirrors the mixer's active-block bookkeeping */
    gains[0] = 0;
    eq_dsp_tick(&s, gains, 1);
    assert(s.gain[0] == 0 && s.was_active == 1);
    for (i = 0; i < BLK; i++) a[i] = original[i];
    eq_dsp_process(&s, 0, a, BLK);
    assert(!memcmp(a, original, sizeof a));
    assert(s.hist[0][0].x1 == original[BLK-1]);
    assert(s.hist[0][0].y1 == original[BLK-1]);
    gains[0] = -1;
    eq_dsp_tick(&s, gains, 1);
    assert(s.gain[0] == -1);
    puts("PASS EQ zero-crossing unity history");
    /* Both LOW shelf polarities must decay rather than accumulate DC after
     * a gated note. This tests stability without assuming a monotonic
     * impulse response (the restored LOW filter has two real poles). */
    for (int db = -15; db <= 15; db += 30) {
        eq_dsp_init(&s);
        gains[0] = (int8_t)db;
        for (i = 0; i < 20; i++) eq_dsp_tick(&s, gains, 1);
        for (i = 0; i < BLK; i++) a[i] = 0;
        a[0] = 1000000;
        eq_dsp_process(&s, 0, a, BLK);
        for (unsigned block = 0; block < 1200; block++) {
            memset(a, 0, sizeof a);
            eq_dsp_process(&s, 0, a, BLK);
        }
        for (i = 0; i < BLK; i++)
            assert(llabs((long long)a[i]) <= 16);
    }
    puts("PASS LOW impulse tail settles for boost and cut");
    check_low_fractional_feedback(-15);
    check_low_fractional_feedback(15);
    return 0;
}
