/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 20B: DRUM-LAB's four SOUND pages. The existing DRUM-X UI remains
 * untouched. On each page KNOB 1 selects one of the 12 independent pads,
 * KNOB 2..4 edit that pad's own synthesis controls.
 */
static const char *const LAB_FIELDS[10] = {
    "PITCH", "SWEEP", "BODY", "SNAP", "NOISE", "TONE", "DRIVE", "WIRES", "WEIGHT", "METAL"
};
static uint8_t lab_ui_seen = 0xFFu;

static void lab_follow(void)
{
    if (lab_ui_seen != last_note) {
        if (lab_ui_seen != 0xFFu)
            dx_ui_lane = (uint8_t)drumlab_lane(last_note);
        lab_ui_seen = last_note;
    }
}

static uint32_t lab_page_index(const page_t *pg)
{
    uint32_t page = dx_pagen(pg);
    return page == 8u ? 3u : page >= 1u && page <= 3u ? page - 1u : 0u;
}

static void lab_knob(const page_t *pg, uint32_t k, int32_t steps)
{
    uint32_t field = lab_page_index(pg) * 3u + k - 1u;
    dlab_patch_t *patch;
    if (k == 0u) {
        dx_ui_lane = (uint8_t)clamp((int32_t)dx_ui_lane + (steps > 0 ? 1 : -1), 0, 11);
        return;
    }
    if (field == 10u) {
        patch = drumlab_patch_of(TSEL, 36u + dx_ui_lane);
        if (patch) {
            int32_t kind = (int32_t)patch->kind;
            kind = clamp(kind + (steps > 0 ? 1 : -1), DLAB_HEAVY_SNARE, DLAB_METAL);
            drumlab_type_set(TSEL, dx_ui_lane, (uint32_t)kind);
        }
        return;
    }
    if (field >= 10u) return;
    patch = drumlab_patch_of(TSEL, 36u + dx_ui_lane);
    if (patch) {
        uint8_t *v = &patch->pitch;
        drumlab_patch_set(TSEL, dx_ui_lane, field,
                          (uint32_t)clamp((int32_t)v[field] + accel(EN_K1 + k, steps, 127), 0, 127));
    }
}

static void lab_cards(const page_t *pg)
{
    uint32_t k, first = lab_page_index(pg) * 3u;
    dlab_patch_t *patch;
    char val[12];
    lab_follow();
    patch = drumlab_patch_of(TSEL, 36u + dx_ui_lane);
    char pad_name[12];
    drumlab_pad_label(dx_ui_lane, patch ? patch->kind : DLAB_KICK, pad_name);
    draw_column(0, "SOUND", pad_name, "", VAL(0u),
                (int32_t)dx_ui_lane * 1000 / 11, ICON_X_PATTERN);
    for (k = 0; k < 3u; k++) {
        uint32_t field = first + k;
        if (patch && field < 10u) {
            uint8_t *v = &patch->pitch;
            fmt_int(val, v[field]);
            draw_column(k + 1u, LAB_FIELDS[field], val, "", VAL(k + 1u),
                        (int32_t)v[field] * 1000 / 127, ICON_LEVEL);
        } else if (patch && field == 10u) {
            draw_column(k + 1u, "TYPE", drumlab_type_name(patch->kind), "", VAL(k + 1u),
                        (int32_t)patch->kind * 1000 / (DLAB_KIND_COUNT - 1), ICON_LEVEL);
        } else {
            draw_column(k + 1u, "", "", "", T_DIM, -1, ICON_NONE);
        }
    }
}
