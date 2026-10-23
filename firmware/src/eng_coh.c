/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Felucca-Sog contributors */
/* KOGERENT (coherence; on the device COHERE: the PRESETS screen has room for six capitals): a stack of 2..5 detuned band-limited saws per voice that falls apart on every note and
 * pulls itself back into step. A note (or a retrigger) knocks the voice's coherence down by HIT; it recovers towards
 * 1 with REC as the time constant (each voice of the part a little faster or slower: the chord re-forms unevenly).
 * While it is low, the stack's spread opens from the locked spread to SPRD, and WEAR holds samples (decimation) and
 * drops bits, and DROP gates short stretches out (a stutter). On the way back a Kuramoto pull (PULL) hauls the phases
 * into step and lets go again: a bump that peaks halfway through the recovery. The saws then drift apart (DRFT: each
 * one's own slow random walk), and that drift is the chorus.
 * Fixed point. The pull is applied once per block as a correction of each saw's frequency: within a block the
 * phase differences of a stack only a few cents apart barely move, so sin(psi - phi_k) is constant enough; the mean
 * field (psi, r) comes from the sums of the saws' cos and sin (no atan2: sin(psi - phi) = (Sy cos phi - Sx sin phi) / R).
 * The per-sample loop is the saws (dsp.c osc_saw, polyBLEP), the gain, the tone filter and the amp; the damage (hold,
 * bits, gate) runs in its own loop (coh_render_d) only while a voice is damaged.
 * State: per voice a coh_v_t in the part's engine memory (engines.c eng_mem). v->s[0] is 1 once the voice has sounded
 * (kept on a retrigger: .keep), so a retrigger only collapses the stack and a new voice also scatters its phases. */
#define COH_NOSC 5
#define COH_TONE (100 << 8)      /* the tone filter's cutoff at rest (of 127 << 8): open, a little off the top */

typedef struct {
    uint32_t ph[COH_NOSC];       /* the saws' phases */
    int16_t ou[COH_NOSC];        /* drift: a random walk with a pull back to 0 (Q15) */
    int16_t out[COH_NOSC];       /* .. its target, a new one every 8 blocks */
    int32_t coh;                 /* coherence, Q15: 32767 locked, HIT knocks it down */
    int32_t spr;                 /* the spread in use, 1/16 cent (smoothed) */
    int32_t kq;                  /* the pull in use, a fraction of the increment, Q16 (smoothed) */
    int32_t norm;                /* the stack's gain, Q14 (smoothed: the level stays put as the stack locks) */
    int32_t hold;                /* decimation: the hold length in use, Q8 samples (smoothed) */
    int32_t hacc, held;          /* .. its accumulator (Q8), the sample held */
    int32_t gate, gtgt, grain;   /* dropout: the gate (Q15), its target, samples to the next decision */
    int32_t dcx, dcy;            /* DC blocker */
    int32_t ic1, ic2;            /* tone filter */
    int32_t rng;
    uint32_t blk;                /* blocks since the voice started */
} coh_v_t;
typedef struct {
    coh_v_t v[NVOICE];
} coh_part_t;
ENG_MEM_FITS("KOGERENT", sizeof(coh_part_t));
static coh_v_t *coh_of(track_t *t, voice_t *v) { return &((coh_part_t *)ENG_MEM((uint32_t)(t - trk)))->v[v - t->v]; }

/* where saw k of n sits in the stack, Q8 (-256..256): even steps, each nudged off the grid by a fixed amount (the
 * golden ratio's fractions), so the stack is never a mirror image and its beating stays uneven */
static const int16_t COH_JIT[COH_NOSC] = {-31, 7, -16, 22, -2};
static inline int32_t coh_det(uint32_t k, uint32_t n) { return (int32_t)((2u * k - (n - 1u)) * 256u / (n - 1u)) + COH_JIT[k]; }

static uint32_t coh_isqrt(uint32_t x)          /* floor(sqrt(x)) */
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

static void coh_note_on(track_t *t, voice_t *v)
{
    coh_v_t *c = coh_of(t, v);
    v->s[0] = 0;                 /* 0: a new voice (render scatters it); a retrigger gets its 1 back (.keep) */
    c->coh = 32767 - t->p[P_E2] * 258;                   /* HIT 127: coherence ~0 */
    if (c->coh < 0)
        c->coh = 0;
}

/* a voice's first block: phases scattered, the stack fully spread, the filters and the damage at rest */
static void coh_start(track_t *t, voice_t *v, coh_v_t *c)
{
    int32_t coh = c->coh;
    uint32_t k;
    memset(c, 0, sizeof *c);
    c->coh = coh;
    c->rng = (int32_t)(v->age * 2654435761u) | 1;
    for (k = 0; k < COH_NOSC; k++) {
        c->ph[k] = noise32(&c->rng);
        c->out[k] = (int16_t)(noise32(&c->rng) >> 17) - 16384;
    }
    c->spr = (10 + t->p[P_E1] * 150 / 127) << 4;
    c->norm = 4096;
    c->hold = 256;
    c->gate = c->gtgt = 32767;
    v->s[0] = 1;
}

/* the block's control values (shared by the two loops) */
typedef struct {
    uint32_t n, inc[COH_NOSC];
    int32_t g0, g1;              /* the gain over the block, Q14 */
    tsvf_t f;
    int32_t hold, qs, dropp;     /* damage: hold Q8, quantiser shift, dropout chance Q15 */
} coh_blk_t;

/* the block's control: coherence, spread, drift, the pull from the mean field, the gain; 1 = the voice is damaged */
static int coh_control(track_t *t, voice_t *v, coh_v_t *c, const vmod_t *m, coh_blk_t *b)
{
    const int16_t *p = t->p;
    uint32_t n = (uint32_t)clamp(p[P_E0], 2, COH_NOSC), k, vi = (uint32_t)(v - t->v);
    int32_t inv, inv2, f, wild, tight, spc, bump, kmul, kq, drift, sx = 0, sy = 0, sc[COH_NOSC], ss[COH_NOSC];
    int32_t r, est, nt, wear;
    uint32_t den, rr, inc16 = m->inc >> 16;
    /* recovery: exponential towards 1, the time constant REC / 3 (the voices of the part 0.66 .. 1.34 of it) */
    den = TIME_MS_X10[p[P_E3] & 127] * 441u / 100u;                /* samples (<= 441000) */
    den = ((den >> 4) * (169u + vi * 174u / (NVOICE - 1u))) >> 4;
    k = den ? ((uint32_t)CTL * 3u << 16) / den : 65536u;
    c->coh += (int32_t)(((uint32_t)(32767 - c->coh) * (k > 65536u ? 65536u : k) + 65535u) >> 16);
    inv = 32767 - c->coh;
    inv2 = (inv * inv) >> 15;
    f = (inv + inv2) >> 1;                                          /* ~ inv^1.35 */
    /* spread, 1/16 cent: the locked one when coherent, SPRD (moved by SHAPE) when not */
    wild = 10 + clamp(p[P_E1] + ((m->shape - (64 << 8)) >> 8), 0, 127) * 150 / 127;
    tight = 2 + wild / 12;
    spc = (tight << 4) + (((wild - tight) << 4) * f >> 15);
    c->spr += (spc - c->spr) >> 3;
    /* the pull: a bump over the recovery, 16 coh^2 (1 - coh)^2 (1 at coh 1/2), x PULL x the loose spread */
    bump = ((((c->coh * c->coh) >> 15) * inv2) >> 15) << 4;
    kmul = 51 + p[P_E4] * 512 / 127;                                /* 0.2 .. 2.2, Q8 */
    kq = (((bump * kmul) >> 8) * wild * 91) >> 15;                  /* Q16 fraction of the increment */
    c->kq += (kq - c->kq) >> 3;
    /* drift: each saw's random walk, its target new every 8 blocks; DRFT 127 = +-7.5 cents */
    drift = p[P_E5] * 120 / 127;                                    /* 1/16 cent */
    for (k = 0; k < n; k++) {
        if (!(c->blk & 7u))
            c->out[k] = (int16_t)((int32_t)(noise32(&c->rng) >> 16) - 32768);
        c->ou[k] = (int16_t)(c->ou[k] + ((c->out[k] - c->ou[k]) >> 9));
        sc[k] = sine_i(c->ph[k] + 0x40000000u);
        ss[k] = sine_i(c->ph[k]);
        sx += sc[k] >> 2;
        sy += ss[k] >> 2;
    }
    c->blk++;
    rr = coh_isqrt((uint32_t)(sx * sx) + (uint32_t)(sy * sy));     /* R: |S| / 4 */
    r = clamp((int32_t)(rr * 4u / n), 0, 32767);                   /* order parameter, Q15 */
    for (k = 0; k < n; k++) {
        int32_t cents = ((coh_det(k, n) * c->spr) >> 8) + ((c->ou[k] * drift) >> 15);   /* 1/16 cent */
        int32_t d = (int32_t)inc16 * ((cents * 2423) >> 10);       /* inc x cents ln2 / 1200 (Q16 fraction) */
        if (c->kq > 0 && rr > 16u) {                                /* sin(psi - phi_k), Q15 */
            int32_t s = ((sy * (sc[k] >> 1) - sx * (ss[k] >> 1)) / (int32_t)rr) << 1;
            d += (int32_t)inc16 * ((c->kq * s) >> 15);
        }
        b->inc[k] = m->inc + (uint32_t)d;
    }
    b->n = n;
    /* the gain: a stack of n saws peaks near n r (the part in step) + sqrt(n (1 - r^2)) (the rest); 0.45 / that */
    est = (int32_t)n * r + (int32_t)coh_isqrt((uint32_t)n * (uint32_t)(32767 - ((r * r) >> 15))) * 181;
    nt = 241591910 / (est < 32768 ? 32768 : est);                   /* 0.45 x 2^14 x 2^15 / est */
    b->g0 = c->norm;
    c->norm += (nt - c->norm) >> 3;
    b->g1 = c->norm;
    tsvf_coef(&b->f, COH_TONE + m->cutoff, 20);          /* (m->cutoff: the LFO / ENV -> FILTER offset, 0 at rest) */
    /* the damage: WEAR holds samples (up to ~42) and drops bits (16 -> ~4), DROP gates out stretches; all with the
     * coherence lost, gone again as the stack recovers */
    wear = p[P_E6] * 258;                                           /* Q15 */
    c->hold += (256 + ((((inv2 * wear) >> 15) * 10650) >> 15) - c->hold) >> 3;   /* 1 + inv^2 WEAR 41.6, Q8 */
    b->hold = c->hold;
    b->qs = (((f * wear) >> 15) * 12) >> 15;
    b->dropp = (((inv2 * p[P_E7] * 258) >> 15) * 29491) >> 15;     /* x 0.9 */
    return b->hold > 261 || b->qs > 0 || b->dropp > 0 || c->gate < 32767;
}

static inline int32_t coh_stack(uint32_t *ph, const uint32_t *inc, uint32_t n)
{
    int32_t sum = 0;
    uint32_t k;
    for (k = 0; k < n; k++) {
        ph[k] += inc[k];
        sum += osc_saw(ph[k], inc[k]);
    }
    return sum;
}

/* a damaged voice: the stack, then hold, bits, the gate, a DC blocker (the quantiser and the gate offset) */
static __attribute__((noinline)) void coh_render_d(coh_v_t *c, const coh_blk_t *b, int32_t *out, uint32_t n,
                                                    const vmod_t *m)
{
    uint32_t i, ph[COH_NOSC];
    int32_t g = b->g0, dg = (b->g1 - b->g0) >> CTL_LOG2, ic1 = c->ic1, ic2 = c->ic2;
    int32_t hacc = c->hacc, held = c->held, gate = c->gate, gtgt = c->gtgt, grain = c->grain;
    int32_t dcx = c->dcx, dcy = c->dcy, hold = b->hold, qs = b->qs;
    memcpy(ph, c->ph, sizeof ph);
    for (i = 0; i < n; i++) {
        int32_t y = ((coh_stack(ph, b->inc, b->n) >> 1) * g) >> 13;
        g += dg;
        hacc += 256;
        if (hacc >= hold) {
            hacc -= hold;
            held = y;
        }
        y = hold > 261 ? held : y;
        if (qs)
            y = ((y + (1 << (qs - 1))) >> qs) << qs;
        if (--grain <= 0) {
            grain = 48 + (int32_t)(noise32(&c->rng) % 420u);
            gtgt = (int32_t)(noise32(&c->rng) >> 17) < b->dropp ? 0 : 32767;
        }
        gate += (gtgt - gate) >> 6;
        y = (y * gate) >> 15;
        dcy = y - dcx + dcy - (dcy >> 10);
        dcx = y;
        y = tsvf_lp(&b->f, dcy, &ic1, &ic2);
        out[i] += voice_amp(clamp(y, -32767, 32767), m, i) << 1;
    }
    memcpy(c->ph, ph, sizeof ph);
    c->ic1 = ic1;
    c->ic2 = ic2;
    c->hacc = hacc;
    c->held = held;
    c->gate = gate;
    c->gtgt = gtgt;
    c->grain = grain;
    c->dcx = dcx;
    c->dcy = dcy;
}

static void coh_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    coh_v_t *c = coh_of(t, v);
    coh_blk_t b;
    uint32_t i, ph[COH_NOSC];
    int32_t g, dg, ic1, ic2;
    if (!v->s[0])
        coh_start(t, v, c);
    if (coh_control(t, v, c, m, &b)) {
        coh_render_d(c, &b, out, n, m);
        return;
    }
    g = b.g0;
    dg = (b.g1 - b.g0) >> CTL_LOG2;
    ic1 = c->ic1;
    ic2 = c->ic2;
    memcpy(ph, c->ph, sizeof ph);
    for (i = 0; i < n; i++) {
        int32_t y = ((coh_stack(ph, b.inc, b.n) >> 1) * g) >> 13;
        g += dg;
        y = tsvf_lp(&b.f, y, &ic1, &ic2);
        out[i] += voice_amp(clamp(y, -32767, 32767), m, i) << 1;
    }
    memcpy(c->ph, ph, sizeof ph);
    c->ic1 = ic1;
    c->ic2 = ic2;
    c->dcx = c->dcy = 0;
}

static const preset_t COH_PRESETS[] = {
    /* name, {OSC, SPRD, HIT, REC, PULL, DRFT, WEAR, DROP}, {A D S R}, fenv, mono */
    {"COLLAPSE", {4, 58, 117, 93, 51, 70, 80, 70}, {0, 90, 110, 60}, 0, 0, FX(0, 0, 30, 50)},
    {"CHOIR", {5, 40, 70, 100, 70, 90, 20, 0}, {30, 90, 120, 80}, 0, 0, FX(0, 20, 20, 70)},
    {"TAPE", {3, 30, 100, 86, 40, 110, 110, 40}, {0, 80, 100, 50}, 0, 0, FX(10, 0, 40, 40)},
    {"LOCK", {4, 90, 127, 80, 127, 30, 50, 90}, {0, 70, 90, 40}, 0, 1, FX(0, 0, 30, 30)},
};

static const engine_t ENG_KOGERENT = {
    .name = "COHERE",
    .page_title = {"COH", "WEAR"},
    .edit = {
        {"OSC", F_INT, 2, COH_NOSC, 4, 0, "osc"},
        {"SPRD", F_PCT, 0, 127, 58, 0, 0},     /* the loose spread: 10 .. 160 cents */
        {"HIT", F_PCT, 0, 127, 117, 0, 0},     /* how far a note knocks the coherence down */
        {"REC", F_TIME, 0, 127, 93, 0, 0},     /* recovery (the time constant x 3) */
        {"PULL", F_PCT, 0, 127, 51, 0, 0},     /* the re-align force: 0.2 .. 2.2 */
        {"DRFT", F_PCT, 0, 127, 70, 0, 0},
        {"WEAR", F_PCT, 0, 127, 80, 0, 0},     /* hold + bits while incoherent */
        {"DROP", F_PCT, 0, 127, 70, 0, 0},     /* dropouts while incoherent */
    },
    .presets = COH_PRESETS,
    .npresets = NELEM(COH_PRESETS),
    .note_on = coh_note_on,
    .render = coh_render,
    .keep = 0x01,                /* s[0]: the voice has sounded (a retrigger collapses, it does not restart) */
    .knob = {P_E2, P_E3, P_E4, P_E6},
    .mem = sizeof(coh_part_t),
};
