/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM-80: twelve-pad edit UI using four SOUND pages (three fields/page)
 * and X-MOD as a dedicated TYPE page. K1 selects pad throughout.
 */
static const char *const D80_FIELDS[D80_PARAMS] = {
    "TUNE","SWEEP","BODY","DECAY","ATTACK","NOISE",
    "BRIGHT","TAIL","DRIVE","PUNCH","VEL","LEVEL"
};
static uint32_t d80_page(const page_t *pg)
{
    uint32_t n = dx_pagen(pg);
    if (n == 4u) return 4u; /* X-MOD => TYPE */
    return n == 8u ? 3u : n >= 1u && n <= 3u ? n - 1u : 0u;
}
static void d80_knob(const page_t *pg, uint32_t k, int32_t steps)
{
    uint32_t ti = song.sel, page = d80_page(pg), pad = dx_ui_lane;
    int32_t delta = steps > 0 ? 1 : -1;
    if (k == 0u) {
        dx_ui_lane = (uint8_t)clamp((int32_t)pad + delta, 0, D80_PADS - 1);
        ui.force = 1u;
        return;
    }
    d80_ensure(ti);
    if (page == 4u) {
        if (k == 1u) {
            uint32_t type = (uint32_t)clamp((int32_t)d80_kit[ti][pad].type + delta, 0, D80_TYPES - 1);
            fm1_irq_off();
            d80_type_set(ti, pad, type);
            fm1_irq_on();
            ui.force = 1u;
        }
        return;
    }
    if (k <= 3u) {
        uint32_t field = page * 3u + k - 1u;
        int32_t value = d80_kit[ti][pad].param[field];
        fm1_irq_off();
        d80_param_set(ti, pad, field, (uint32_t)clamp(value + accel(EN_K1 + k, steps, 127), 0, 127));
        fm1_irq_on();
        ui.force = 1u;
    }
}
static void d80_cards(const page_t *pg)
{
    uint32_t ti = song.sel, pad = dx_ui_lane, page = d80_page(pg);
    char value[12], label[12];
    d80_ensure(ti);
    str_cpy(label, d80_names[d80_kit[ti][pad].type], sizeof label);
    draw_column(0, "PAD", label, "", VAL(0u), (int32_t)pad * 1000 / 11, ICON_X_PATTERN);
    for (uint32_t k = 1u; k < 4u; k++) {
        if (page == 4u) {
            if (k == 1u)
                draw_column(k, "TYPE", d80_names[d80_kit[ti][pad].type], "", VAL(k),
                            (int32_t)d80_kit[ti][pad].type * 1000 / (D80_TYPES - 1), ICON_LEVEL);
            else draw_column(k, "", "", "", T_DIM, -1, ICON_NONE);
        } else {
            uint32_t field = page * 3u + k - 1u;
            uint32_t v = d80_kit[ti][pad].param[field];
            fmt_int(value, v);
            draw_column(k, D80_FIELDS[field], value, "", VAL(k), (int32_t)v * 1000 / 127, ICON_LEVEL);
        }
    }
}
