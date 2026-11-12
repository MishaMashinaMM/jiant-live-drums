/* GPL-3.0-only: JLD three-band track equalizer, fixed-point coefficients.
 * Audio rate 44117.647 Hz (FS); RBJ broad shelves S=0.25 (50 Hz / 10 kHz), MID Q=1.
 * Stored gains -15..+15 dB; coefficient table indexed by gain + 15.
 * This module is independent until its mixer/UI/project integration is tested. */
#include <stdint.h>
#include <limits.h>
#include "track_eq_dsp.h"

typedef struct { int32_t b0, b1, b2, a1, a2; } eq_coef_t;
/* 50 Hz broad low shelf: RBJ S=0.25, restored after one-pole experiment. */
static const eq_coef_t EQ_LOW[31] = {
    {265734488, -527533773, 261804926, -527520735, 259116996}, /* -15 dB */
    {265961987, -527934091, 261978083, -527922097, 259516607}, /* -14 dB */
    {266179457, -528310316, 262137197, -528299330, 259892184}, /* -13 dB */
    {266387689, -528663692, 262282721, -528653679, 260244967}, /* -12 dB */
    {266587456, -528995403, 262415067, -528986331, 260576140}, /* -11 dB */
    {266779516, -529306581, 262534612, -529298422, 260886831}, /* -10 dB */
    {266964612, -529598306, 262641692, -529591034, 261178121}, /* -9 dB */
    {267143478, -529871612, 262736610, -529865204, 261451040}, /* -8 dB */
    {267316837, -530127484, 262819631, -530121920, 261706575}, /* -7 dB */
    {267485402, -530366868, 262890985, -530362130, 261945669}, /* -6 dB */
    {267649882, -530590665, 262950870, -530586739, 262169222}, /* -5 dB */
    {267810979, -530799739, 262999450, -530796612, 262378099}, /* -4 dB */
    {267969391, -530994916, 263036853, -530992580, 262573125}, /* -3 dB */
    {268125812, -531176988, 263063180, -531175434, 262755089}, /* -2 dB */
    {268280936, -531346711, 263078493, -531345935, 262924749}, /* -1 dB */
    {268435456, -531504808, 263082828, -531504808, 263082828}, /* 0 dB */
    {268590065, -531651971, 263076184, -531652747, 263230017}, /* 1 dB */
    {268745458, -531788861, 263058531, -531790416, 263366977}, /* 2 dB */
    {268902332, -531916108, 263029805, -531918449, 263494340}, /* 3 dB */
    {269061389, -532034314, 262989908, -532037448, 263612707}, /* 4 dB */
    {269223335, -532144053, 262938710, -532147990, 263722652}, /* 5 dB */
    {269388884, -532245868, 262876046, -532250622, 263824720}, /* 6 dB */
    {269558756, -532340279, 262801718, -532345866, 263919431}, /* 7 dB */
    {269733682, -532427774, 262715488, -532434213, 264007275}, /* 8 dB */
    {269914403, -532508821, 262617084, -532516133, 264088719}, /* 9 dB */
    {270101675, -532583856, 262506194, -532592066, 264164203}, /* 10 dB */
    {270296266, -532653295, 262382468, -532662430, 264234144}, /* 11 dB */
    {270498964, -532717528, 262245514, -532727619, 264298932}, /* 12 dB */
    {270710576, -532776921, 262094896, -532788001, 264358936}, /* 13 dB */
    {270931929, -532831818, 261930134, -532843923, 264414502}, /* 14 dB */
    {271163877, -532882541, 261750703, -532895712, 264465953}, /* 15 dB */
};

static const eq_coef_t EQ_MID[31] = {
    {245140062, -477818693, 235062921, -477818693, 211767528}, /* -15 dB */
    {246896416, -480656730, 236158767, -480656730, 214619727}, /* -14 dB */
    {248608566, -483367122, 237170534, -483367122, 217343644}, /* -13 dB */
    {250279803, -485954098, 238099182, -485954098, 219943528}, /* -12 dB */
    {251913496, -488421900, 238945605, -488421900, 222423645}, /* -11 dB */
    {253513083, -490774767, 239710625, -490774767, 224788252}, /* -10 dB */
    {255082071, -493016914, 240394972, -493016914, 227041587}, /* -9 dB */
    {256624029, -495152518, 240999275, -495152518, 229187848}, /* -8 dB */
    {258142588, -497185706, 241524050, -497185706, 231231182}, /* -7 dB */
    {259641441, -499120542, 241969687, -499120542, 233175672}, /* -6 dB */
    {261124340, -500961014, 242336445, -500961014, 235025329}, /* -5 dB */
    {262595102, -502711032, 242624433, -502711032, 236784079}, /* -4 dB */
    {264057608, -504374412, 242833607, -504374412, 238455759}, /* -3 dB */
    {265515807, -505954876, 242963759, -505954876, 240044110}, /* -2 dB */
    {266973723, -507456047, 243014505, -507456047, 241552772}, /* -1 dB */
    {268435456, -508881442, 242985279, -508881442, 242985279}, /* 0 dB */
    {269905193, -510234469, 242875320, -510234469, 244345057}, /* 1 dB */
    {271387210, -511518427, 242683669, -511518427, 245635423}, /* 2 dB */
    {272885885, -512736506, 242409150, -512736506, 246859580}, /* 3 dB */
    {274405705, -513891783, 242050372, -513891783, 248020621}, /* 4 dB */
    {275951273, -514987222, 241605709, -514987222, 249121526}, /* 5 dB */
    {277527323, -516025677, 241073296, -516025677, 250165163}, /* 6 dB */
    {279138729, -517009892, 240451017, -517009892, 251154290}, /* 7 dB */
    {280790518, -517942503, 239736492, -517942503, 252091554}, /* 8 dB */
    {282487882, -518826037, 238927071, -518826037, 252979497}, /* 9 dB */
    {284236195, -519662917, 238019815, -519662917, 253820553}, /* 10 dB */
    {286041023, -520455465, 237011488, -520455465, 254617056}, /* 11 dB */
    {287908146, -521205900, 235898545, -521205900, 255371235}, /* 12 dB */
    {289843569, -521916344, 234677112, -521916344, 256085225}, /* 13 dB */
    {291853544, -522588827, 233342975, -522588827, 256761063}, /* 14 dB */
    {293944585, -523225283, 231891566, -523225283, 257400695}, /* 15 dB */
};

static const eq_coef_t EQ_HIGH[31] = {
    {107849592, 23464915, -23186452, -118007427, -42299974}, /* -15 dB */
    {114557223, 22628681, -24179371, -113271832, -42157091}, /* -14 dB */
    {121692180, 21535594, -25211891, -108374886, -42044687}, /* -13 dB */
    {129281977, 20155541, -26287264, -103319214, -41965989}, /* -12 dB */
    {137355822, 18456261, -27409294, -98108464, -41924203}, /* -11 dB */
    {145944697, 16403322, -28582392, -92747354, -41922474}, /* -10 dB */
    {155081453, 13960109, -29811652, -87241713, -41963832}, /* -9 dB */
    {164800896, 11087829, -31102900, -81598494, -42051136}, /* -8 dB */
    {175139883, 7745524, -32462764, -75825785, -42187028}, /* -7 dB */
    {186137426, 3890099, -33898726, -69932787, -42373869}, /* -6 dB */
    {197834790, -523646, -35419168, -63929785, -42613695}, /* -5 dB */
    {210275605, -5542972, -37033420, -57828083, -42908160}, /* -4 dB */
    {223505983, -11217163, -38751793, -51639933, -43258495}, /* -3 dB */
    {237574639, -17597481, -40585604, -45378432, -43665470}, /* -2 dB */
    {252533022, -24737137, -42547197, -39057403, -44129365}, /* -1 dB */
    {268435456, -32691265, -44649948, -32691265, -44649948}, /* 0 dB */
    {285339294, -41516915, -46908266, -26294877, -45226467}, /* 1 dB */
    {303305076, -51273066, -49337591, -19883384, -45857652}, /* 2 dB */
    {322396712, -62020662, -51954376, -13472052, -46541730}, /* 3 dB */
    {342681663, -73822676, -54776071, -7076095, -47276444}, /* 4 dB */
    {364231155, -86744202, -57821108, -710518, -48059092}, /* 5 dB */
    {387120396, -100852580, -61108876, 5610051, -48886568}, /* 6 dB */
    {411428811, -116217556, -64659710, 11871501, -49755411}, /* 7 dB */
    {437240305, -132911468, -68494870, 18060378, -50661867}, /* 8 dB */
    {464643530, -151009476, -72636540, 24164001, -51601943}, /* 9 dB */
    {493732183, -170589811, -77107828, 30170560, -52571472}, /* 10 dB */
    {524605315, -191734066, -81932768, 36069202, -53566177}, /* 11 dB */
    {557367669, -214527508, -87136348, 41850086, -54581729}, /* 12 dB */
    {592130030, -239059420, -92744537, 47504425, -55613807}, /* 13 dB */
    {629009607, -265423471, -98784325, 53024507, -56658152}, /* 14 dB */
    {668130427, -293718103, -105283781, 58403699, -57710611}, /* 15 dB */
};

static const eq_coef_t *const EQ_TABLES[3] = {EQ_LOW, EQ_MID, EQ_HIGH};

void eq_dsp_init(eq_dsp_t *s)
{
    unsigned i, j, k;
    for (i = 0; i < EQ_STREAMS; i++) for (j = 0; j < EQ_BANDS; j++) {
        eq_hist_t *h = &s->hist[i][j];
        h->x1 = h->x2 = h->y1 = h->y2 = 0;
    }
    for (k = 0; k < EQ_BANDS; k++) s->gain[k] = 0;
    s->on = 1;
    s->was_active = 0;
    for (i = 0; i < EQ_STREAMS; i++)
        s->low_frac[i][0] = s->low_frac[i][1] = 0;
#if defined(EQ_DIAG_LOW_DOUBLE) && EQ_DIAG_LOW_DOUBLE
    for (i = 0; i < EQ_STREAMS; i++)
        for (j = 0; j < 4; j++)
            s->low_hist[i][j] = 0.0;
#endif
}

/* Call once per track block, never once per bus: do not accelerate smoothing
 * when rendering stereo/pad-send paths. The audio block sees stable coefficients. */
void eq_dsp_tick(eq_dsp_t *s, const int8_t gain[3], uint8_t on)
{
    unsigned i;
    s->on = !!on;
    for (i = 0; i < EQ_BANDS; i++) {
        int g = s->on ? gain[i] : 0;
        if (g < -15) g = -15;
        if (g > 15) g = 15;
        if (s->gain[i] < g) s->gain[i]++;
        else if (s->gain[i] > g) s->gain[i]--;
    }
}

static int32_t eq_sat(int64_t x)
{
    return x > INT32_MAX ? INT32_MAX : x < INT32_MIN ? INT32_MIN : (int32_t)x;
}

/* Apply three biquads to one of the track's isolated streams. The same EQ can
 * filter dry L, dry R and each of three sends before they're mixed globally.
 * When a band is neutral, maintain unity history without costly multiplies. */
void eq_dsp_process(eq_dsp_t *s, unsigned stream, int32_t *buf, unsigned n)
{
    unsigned band, i;
    if (stream >= EQ_STREAMS || !buf) return;
    /* A zero send with settled histories is free: typical live sets enable
     * only one or two sends, and calculating three filters for silence would
     * otherwise be a substantial waste on the FM-1's two CPU cores. */
    for (band = 0; band < EQ_BANDS; band++) {
        const eq_hist_t *h = &s->hist[stream][band];
        if (h->x1 || h->x2 || h->y1 || h->y2 ||
            (band == 0 && (s->low_frac[stream][0] || s->low_frac[stream][1]))) break;
    }
    if (band == EQ_BANDS) {
        for (i = 0; i < n && !buf[i]; i++) {}
        if (i == n) return;
    }
    for (band = 0; band < EQ_BANDS; band++) {
        eq_hist_t *h = &s->hist[stream][band];
        const eq_coef_t *c = &EQ_TABLES[band][s->gain[band] + 15];
        /* Emulator-only diagnostic: bypass LOW's arithmetic while preserving
         * the entire per-track mixer route and EQ-active bookkeeping.
         * Build with EMU_CFLAGS=-DEQ_DIAG_BYPASS_LOW=1; normal builds (including
         * hardware and tests) retain the real 50 Hz shelf. */
#if defined(EQ_DIAG_BYPASS_LOW) && EQ_DIAG_BYPASS_LOW
        if (!s->gain[band] || band == 0) {
#else
        if (!s->gain[band]) {
#endif
            for (i = 0; i < n; i++) {
                int32_t x = buf[i];
                h->x2 = h->x1; h->x1 = x;
                h->y2 = h->y1; h->y1 = x;
                if (band == 0)
                    s->low_frac[stream][0] = s->low_frac[stream][1] = 0;
#if defined(EQ_DIAG_LOW_DOUBLE) && EQ_DIAG_LOW_DOUBLE
                if (band == 0) {
                    double *q = s->low_hist[stream];
                    q[1] = q[0]; q[0] = (double)x;
                    q[3] = q[2]; q[2] = (double)x;
                }
#endif
            }
            continue;
        }
#if defined(EQ_DIAG_LOW_DOUBLE) && EQ_DIAG_LOW_DOUBLE
        if (band == 0) {
            /* Diagnostic only: same Q28 shelf coefficients and audio routing,
             * but floating-point recursive state avoids per-sample integer
             * truncation at the 50 Hz pole. Never use on the FM1 hardware:
             * this is to isolate the crackle's numerical origin. */
            const double scale = 1.0 / 268435456.0;
            double *q = s->low_hist[stream];
            for (i = 0; i < n; i++) {
                int32_t x = buf[i];
                double y = ((double)c->b0 * (double)x +
                            (double)c->b1 * q[0] +
                            (double)c->b2 * q[1] -
                            (double)c->a1 * q[2] -
                            (double)c->a2 * q[3]) * scale;
                if (y > (double)INT32_MAX) y = (double)INT32_MAX;
                if (y < (double)INT32_MIN) y = (double)INT32_MIN;
                q[1] = q[0]; q[0] = (double)x;
                q[3] = q[2]; q[2] = y;
                buf[i] = (int32_t)y;
                h->x2 = h->x1; h->x1 = x;
                h->y2 = h->y1; h->y1 = buf[i];
            }
            continue;
        }
#endif
        if (band == 0) {
            /* LOW only: preserve the two previous output samples' fractional
             * Q28 remainders in the feedback path. Without these remainders,
             * the near-unity 50 Hz poles repeatedly truncate sub-LSB state,
             * causing audible quantization transients at note boundaries.
             *
             * acc is in Q28. -a*frac has Q56 units, shifted to Q28 before
             * summation. 64-bit products retain 28 fractional bits without
             * floating point; MID and HIGH continue on the original path. */
            int32_t *frac = s->low_frac[stream];
            for (i = 0; i < n; i++) {
                int32_t x = buf[i];
                int64_t feedback = (int64_t)c->a1 * frac[0] +
                                   (int64_t)c->a2 * frac[1];
                int64_t acc = (int64_t)c->b0 * x +
                              (int64_t)c->b1 * h->x1 +
                              (int64_t)c->b2 * h->x2 -
                              (int64_t)c->a1 * h->y1 -
                              (int64_t)c->a2 * h->y2 -
                              (feedback >> 28);
                int64_t yi = acc >> 28;
                int32_t y = eq_sat(yi);
                int32_t rem = yi == (int64_t)y
                                ? (int32_t)(acc - yi * INT64_C(268435456))
                                : 0;
                frac[1] = frac[0]; frac[0] = rem;
                h->x2 = h->x1; h->x1 = x;
                h->y2 = h->y1; h->y1 = y;
                buf[i] = y;
            }
            continue;
        }
        for (i = 0; i < n; i++) {
            int32_t x = buf[i];
            int64_t acc = (int64_t)c->b0 * x + (int64_t)c->b1 * h->x1 + (int64_t)c->b2 * h->x2
                        - (int64_t)c->a1 * h->y1 - (int64_t)c->a2 * h->y2;
            int32_t y = eq_sat(acc >> 28);
            h->x2 = h->x1; h->x1 = x;
            h->y2 = h->y1; h->y1 = y;
            buf[i] = y;
        }
    }
}
