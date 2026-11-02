/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Felucca-Sog contributors */
/* The big view (Felucca-Sog; after ChoralRoot FM-1's screens by Quixotic7, GPL-3.0): what a knob changes, over the
 * whole screen while it moves (ui.hot_t, as the card's emphasis): the value in the X face (96 px; in B, 40 px, when it
 * has a character X lacks or is too wide), its name under it in B, a bar of 12 segments where it sits in its range
 * (a bipolar one from the middle), the track and its engine on top. Three kinds:
 *   BIG_VALUE  a page's or HOME's knob: that parameter, live (its value pointer)
 *   BIG_LOCK   a step held on SEQ > STEP turning a knob (a parameter lock, ui_input.c lock_turn): the lock's value,
 *              LOCK and the step on top, in ACCENT
 *   BIG_SOUND  PRESETS (preset_step): the sound's number in its engine and its name, the engine on top
 * Over the page (ui_draw.c ui_draw_page), not over a layer, the MENU, a dialog or NAME. Drawn in two bands of 120 rows
 * (the canvas holds 124). big_on: on; tests/ui_test.c's checks of the pages under a turned knob run with it off. */
enum { BIG_NONE, BIG_VALUE, BIG_LOCK, BIG_SOUND };
#define BIG_SEGS 12u
static struct {
    uint8_t kind, step, tr;           /* step: BIG_LOCK's (the lowest held); tr: the track */
    uint8_t id;                       /* BIG_LOCK: the locked parameter */
    const param_desc_t *d;            /* BIG_VALUE: the parameter and its value */
    const int16_t *vp;
} big;
static uint8_t big_on = 1;

static void big_value(const track_t *t, const param_desc_t *d, const int16_t *vp)
{
    if (!d || !vp)
        return;
    big.kind = BIG_VALUE;
    big.tr = (uint8_t)((uint32_t)(t - trk) % NTRK);
    big.d = d;
    big.vp = vp;
}
static void big_lock(uint32_t id, uint64_t held)
{
    uint32_t i;
    for (i = 0; i < NSTEP && !((held >> i) & 1u); i++)
        ;
    big.kind = BIG_LOCK;
    big.tr = song.sel % NTRK;
    big.id = (uint8_t)id;
    big.step = (uint8_t)(i < NSTEP ? i : 0u);
}
static void big_sound(void)
{
    big.kind = BIG_SOUND;
    big.tr = song.sel % NTRK;
}
static int big_showing(void) { return big_on && big.kind != BIG_NONE && ui.hot_t; }

/* s drawn in X: every character in it (a space too) and at most 224 px wide */
static int big_x_fits(const char *s)
{
    const char *p;
    for (p = s; *p; p++)
        if (*p != ' ' && glyph_at(&AF_X, fold(&AF_X, (uint8_t)*p)) < 0)
            return 0;
    return text_w(&AF_X, s) <= 224;
}

/* centred text in f with its capitals' middle at screen row cy, the band starting at screen row oy */
static void big_text(const aafont_t *f, int32_t cap_y, int32_t cap_h, const char *s, int32_t cy, uint16_t c, int32_t oy)
{
    int32_t w = text_w(f, s);
    cv_text(120 - w / 2, cy - cap_h / 2 - cap_y - oy, f, s, c);
}
static void big_text_fit(const char *s, int32_t cy, uint16_t c, int32_t oy)
{
    if (big_x_fits(s))
        big_text(&AF_X, AF_X_CAP_Y, AF_X_CAP_H, s, cy, c, oy);
    else if (text_w(&AF_B, s) <= 224)
        big_text(&AF_B, AF_B_CAP_Y, AF_B_CAP_H, s, cy, c, oy);
    else
        big_text(&AF_M, AF_M_CAP_Y, AF_M_CAP_H, s, cy, c, oy);
}

/* the bar: BIG_SEGS segments at screen row 196, the first `on` lit (bipolar: from the middle to `pos`) */
static void big_bar(int32_t pos, int bip, uint16_t c, int32_t oy)
{
    uint32_t i;
    for (i = 0; i < BIG_SEGS; i++) {
        int32_t x = 8 + (int32_t)i * 19, lit = bip ? (pos >= 6 ? (int32_t)i >= 6 && (int32_t)i < pos
                                                                : (int32_t)i >= pos && (int32_t)i < 6)
                                                    : (int32_t)i < pos;
        cv_rect(x, 196 - oy, 15, 10, lit ? c : T_RAISE);
    }
}

static void big_draw(void)
{
    char val[24], label[40], top[24], tag[16];
    const char *unit = "";
    const track_t *t = &trk[big.tr % NTRK];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint16_t vc = T_TEXT, tc = T_MID;
    int32_t pos = 0, band, bip = 0;
    tag[0] = 0;
    str_cpy(top, "T", sizeof top);
    top[1] = (char)('1' + big.tr % NTRK);
    top[2] = ' ';
    str_cpy(top + 3, e->name, sizeof top - 3);
    if (big.kind == BIG_SOUND) {
        uint32_t n = e->npresets ? e->npresets : 1u, k = t->preset % n, u = user_of(t);
        if (u < UP_SLOTS) {
            val[0] = 'U';
            val[1] = (char)('0' + (u + 1u) / 10u);
            val[2] = (char)('0' + (u + 1u) % 10u);
            val[3] = 0;
            up_name(u, label);                         /* (upper case, 0-terminated, 13 bytes) */
            pos = (int32_t)((u * BIG_SEGS + UP_SLOTS / 2u) / UP_SLOTS) + 1;
        } else {
            val[0] = (char)('0' + (k + 1u) / 10u);
            val[1] = (char)('0' + (k + 1u) % 10u);
            val[2] = 0;
            str_cpy(label, e->npresets ? e->presets[k].name : "-", sizeof label);
            pos = (int32_t)(((k + 1u) * BIG_SEGS + n / 2u) / n);
        }
        vc = T_THEME;
    } else {
        const param_desc_t *d;
        int16_t v;
        if (big.kind == BIG_LOCK) {
            d = track_desc(t, big.id);
            if (!motion_lock_get(t, big.step, big.id, &v))
                v = (int16_t)motion_base_value(t, big.id);
            str_cpy(tag, "LOCK S", sizeof tag);
            tag[6] = (char)('0' + (big.step + 1u) / 10u);
            tag[7] = (char)('0' + (big.step + 1u) % 10u);
            tag[8] = 0;
            vc = T_ACCENT;
            tc = T_ACCENT;
        } else {
            d = big.d;
            v = *big.vp;
        }
        param_format(d, v, val, &unit);
        str_cpy(label, d->label, sizeof label);
        if (unit && unit[0]) {
            uint32_t n = 0;
            while (label[n])
                n++;
            if (n + 2u < sizeof label) {
                label[n] = ' ';
                str_cpy(label + n + 1u, unit, sizeof label - n - 1u);
            }
        }
        if (d->max > d->min) {
            bip = d->min < 0 && d->max > 0;
            pos = (int32_t)(((int32_t)(v - d->min) * (int32_t)BIG_SEGS + (d->max - d->min) / 2) / (d->max - d->min));
        }
    }
    for (band = 0; band < 2; band++) {
        int32_t oy = band * 120;
        cv_begin(240, 120, T_BG);
        cv_text(10, 10 - AF_M_CAP_Y - oy, &AF_M, top, T_MID);
        if (tag[0])
            cv_text_r(230, 10 - AF_M_CAP_Y - oy, &AF_M, tag, tc, T_BG);
        big_text_fit(val, 98, vc, oy);
        big_text(&AF_B, AF_B_CAP_Y, AF_B_CAP_H, label, 166, T_THEME, oy);
        big_bar(pos, bip, big.kind == BIG_LOCK ? T_ACCENT : T_THEME, oy);
        cv_blit(0, (uint32_t)oy);
    }
}
