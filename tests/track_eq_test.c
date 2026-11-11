/* Host verification of JLD's standalone, fixed-point track EQ. */
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
    return 0;
}
