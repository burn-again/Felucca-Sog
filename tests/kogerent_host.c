/* SPDX-License-Identifier: GPL-3.0-only */
/* cc -O2 -std=gnu99 -I build/gen -o build/kogerent_host tests/kogerent_host.c -lm && build/kogerent_host build    (after ./build.sh) */
/* Work-in-progress host for eng_coh.c (KOGERENT): renders each preset through a phrase, prints level / coherence stats,
 * writes 16-bit mono WAVs. Built twice: with cc for the renders, with JieLi clang (-DTARGET) for the target cost. */
#include <stdint.h>
#define NVOICE 8
#define NPART 4
#include "felucca_tables.h"
#include "../../firmware/engines/src/felucca_engine.h"
enum { P_ATK, P_REL, P_FTYPE, P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_COUNT };
typedef struct track { int16_t p[P_COUNT]; voice_t v[NVOICE]; } track_t;
track_t trk[NPART];
#ifdef TARGET
void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
#else
#include <stdio.h>
#include <string.h>
#endif
#define ENG_MEM_PART 12888u
static union { uint64_t a; uint8_t b[ENG_MEM_PART]; } eng_mem[NPART];
#define ENG_MEM(p) ((void *)eng_mem[p].b)
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= ENG_MEM_PART, name " state over ENG_MEM_PART")
#include "../../firmware/engines/src/dsp.c"
#include "../../firmware/src/eng_coh.c"

#ifdef TARGET
/* keep the render (and what it calls) in the object for the cost count */
void coh_entry(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m) { ENG_KOGERENT.render(t, v, out, n, m); }
#else
#define NBLK (44100 * 6 / CTL)
static int16_t pcm[NBLK * CTL];
static void wav(const char *path, const int16_t *s, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t d = n * 2, v;
    fwrite("RIFF", 1, 4, f); v = 36 + d; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 1 | (1u << 16); fwrite(&v, 4, 1, f); v = 44100; fwrite(&v, 4, 1, f);
    v = 88200; fwrite(&v, 4, 1, f); v = 2 | (16u << 16); fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f);
    fwrite(&d, 4, 1, f); fwrite(s, 2, n, f); fclose(f);
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t pi;
    for (pi = 0; pi < ENG_KOGERENT.npresets; pi++) {
        const preset_t *pr = &ENG_KOGERENT.presets[pi];
        track_t *t = &trk[0];
        int32_t out[CTL];
        uint32_t b, i, k, age = 0;
        int64_t peak = 0;
        double rms = 0;
        static const uint8_t notes[4] = {45, 52, 57, 60};
        memset(t, 0, sizeof *t);
        memset(eng_mem, 0, sizeof eng_mem);
        for (k = 0; k < 8; k++) t->p[P_E0 + k] = pr->e[k];
        for (b = 0; b < NBLK; b++) {
            /* a chord of 4 at 0 s, retriggered at 2 s, released at 4.5 s */
            if (b == 0 || b == 44100 * 2 / CTL)
                for (k = 0; k < 4; k++) {
                    voice_t *v = &t->v[k];
                    int sounding = v->active;
                    int32_t s0 = v->s[0];
                    v->note = notes[k]; v->active = 1; v->gate = 1; v->stage = 2; v->age = ++age;
                    v->pitch16 = notes[k] * 16;
                    ENG_KOGERENT.note_on(t, v);
                    if (sounding) v->s[0] = s0;
                }
            if (b == 44100 * 9 / 2 / CTL)
                for (k = 0; k < 4; k++) t->v[k].stage = 3;
            for (i = 0; i < CTL; i++) out[i] = 0;
            for (k = 0; k < NVOICE; k++) {
                voice_t *v = &t->v[k];
                vmod_t m = {0};
                int32_t a;
                if (!v->active) continue;
                a = v->stage == 3 ? (int32_t)(32767.0 * (1.0 - (b - 44100 * 9 / 2 / CTL) / 300.0)) : 32767;
                if (a <= 0) { v->active = 0; continue; }
                m.inc = pitch_inc(v->pitch16); m.pitch16 = v->pitch16;
                m.amp0 = m.amp1 = a; m.cutoff = 100 << 8; m.shape = 64 << 8;
                ENG_KOGERENT.render(t, v, out, CTL, &m);
            }
            for (i = 0; i < CTL; i++) {
                int32_t y = out[i] >> 2;          /* 4 voices into 16 bits */
                if ((y < 0 ? -y : y) > peak) peak = y < 0 ? -y : y;
                rms += (double)y * y;
                pcm[b * CTL + i] = (int16_t)(y > 32767 ? 32767 : y < -32768 ? -32768 : y);
            }
            if (b % (44100 / 16 / CTL) == 0 && b < 44100 * 1 / CTL && b < 44100 * 9 / 2 / CTL) {
                coh_v_t *c = &((coh_part_t *)ENG_MEM(0))->v[0];
                { int32_t sx=0,sy=0; uint32_t q; for(q=0;q<(uint32_t)clamp(t->p[P_E0],2,5);q++){sx+=sine_i(c->ph[q]+0x40000000u)>>2;sy+=sine_i(c->ph[q])>>2;} printf("  r=%.2f", 4.0*coh_isqrt((uint32_t)(sx*sx)+(uint32_t)(sy*sy))/32768.0/clamp(t->p[P_E0],2,5)); } printf("  %-9s t=%.2fs coh=%.3f spread=%.1fct pull=%.3f hold=%.1f gain=%.3f\n", pr->name,
                       b * CTL / 44100.0, c->coh / 32768.0, c->spr / 16.0, c->kq / 65536.0, c->hold / 256.0, c->norm / 16384.0);
            }
        }
        {
            char path[512];
            snprintf(path, sizeof path, "%s/kogerent_%s.wav", dir, pr->name);
            wav(path, pcm, NBLK * CTL);
            printf("%-9s peak %.1f dBFS  rms %.1f dBFS  -> %s\n", pr->name, 20 * __builtin_log10(peak / 32768.0 + 1e-9),
                   10 * __builtin_log10(rms / (NBLK * CTL) / (32768.0 * 32768.0) + 1e-12), path);
        }
    }
    return 0;
}
#endif
