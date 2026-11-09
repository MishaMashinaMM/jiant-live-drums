/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 20B: physical SOUND page knob editing is specific to DRUM-LAB. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0;
    uint8_t other, before;
    ui_power_on();
    set_engine_of(TSEL, ENGI_DRUMLAB);
    TSEL->engine = TSEL->eng_req;
    {
        uint32_t xmod = NPAGES;
        for (uint32_t p = 0; p < NPAGES; p++)
            if (str_eq(PAGES[p].title, "X-MOD") && PAGES[p].fam == FAM_EDIT) { xmod = p; break; }
        bad += check("Stage 23A: DRUM-LAB hides DRUM-X's duplicate X-MOD page",
                     xmod < NPAGES && !page_visible(xmod));
    }
    go_title("SOUND"); frame();
    {
        dlab_patch_t original = *drumlab_patch_of(TSEL, 36u);
        dlab_patch_t sounding = original;
        drumlab_global_reset(0u);
        drumlab_global_patch(0u, &sounding);
        bad += check("Stage 25E: eight neutral global kit controls leave pad unchanged",
                     !memcmp(&sounding, &original, sizeof sounding));
        drumlab_global[0][0] = 50u;
        drumlab_global[0][2] = 0u;
        sounding = original;
        drumlab_global_patch(0u, &sounding);
        bad += check("Stage 25E: global pitch and body scale without modifying pad",
                     sounding.pitch == original.pitch / 2u && !sounding.body &&
                     !memcmp(&original, drumlab_patch_of(TSEL, 36u), sizeof original));
        drumlab_global_reset(0u);
        go_title("EDIT 1"); frame();
        ml_set(1u);
        uint32_t pitch_before = drumlab_global[0][0];
        turn(EN_K1, -1); frame();
        bad += check("Stage 25E: STRIKE knob edits global pitch, not pad pitch",
                     drumlab_global[0][0] < pitch_before &&
                     !memcmp(&original, drumlab_patch_of(TSEL, 36u), sizeof original));
        bad += check("Stage 26: global STRIKE knob is picked by existing MIDI CC Learn",
                     ml.pick && ml.trk == 0u && ml.id == P_E0);
        ml_set(0u);
        go_title("EDIT 2"); frame();
        uint32_t noise_before = drumlab_global[0][4];
        turn(EN_K1, -1); frame();
        bad += check("Stage 25E: BODY page knob edits kit noise independently",
                     drumlab_global[0][4] < noise_before && drumlab_global[0][0] < 100u);
        apply_preset(0u);
        bad += check("Stage 25E: loading a kit preset resets all global controls to 100%",
                     drumlab_global[0][0] == 100u && drumlab_global[0][4] == 100u);
        drumlab_global_reset(0u);
        go_title("SOUND"); frame();
    }
    dx_ui_lane = 2u;
    lab_ui_seen = last_note;
    before = drumlab_patch_of(TSEL, 38)->pitch;
    other = drumlab_patch_of(TSEL, 36)->pitch;
    turn(EN_K2, 1); frame();
    bad += check("DRUM-LAB SOUND knob 2 edits only selected snare PITCH",
                 drumlab_patch_of(TSEL, 38)->pitch > before && drumlab_patch_of(TSEL, 36)->pitch == other);
    bad += check("DRUM-LAB SOUND page edit marks only that pad dirty",
                 drumlab_kit_dirty[0] == (1u << 2));
    go_title("SOUND 2"); frame();
    before = drumlab_patch_of(TSEL, 38)->snap;
    turn(EN_K2, 1); frame();
    bad += check("SOUND 2 edits the selected pad's SNAP", drumlab_patch_of(TSEL, 38)->snap > before);
    go_title("SOUND 3"); frame();
    before = drumlab_patch_of(TSEL, 38)->drive;
    turn(EN_K2, -1); frame();
    bad += check("SOUND 3 edits the selected pad's DRIVE", drumlab_patch_of(TSEL, 38)->drive < before);
    go_title("SOUND 4"); frame();
    before = drumlab_patch_of(TSEL, 38)->metal;
    turn(EN_K2, 1); frame();
    bad += check("SOUND 4 edits the selected pad's METAL", drumlab_patch_of(TSEL, 38)->metal > before);
    turn(EN_K3, -1); frame();
    bad += check("Stage 24B fix: SOUND 4 TYPE can change SNARE to KICK",
                 drumlab_patch_of(TSEL, 38)->kind == DLAB_KICK);
    turn(EN_K3, 1); frame();
    bad += check("Stage 24B fix: TYPE advances KICK to approved SNARE",
                 drumlab_patch_of(TSEL, 38)->kind == DLAB_HEAVY_SNARE);
    turn(EN_K3, 1); frame();
    bad += check("Stage 24B: SOUND 4 knob 3 selects CLAP independently",
                 drumlab_patch_of(TSEL, 38)->kind == DLAB_CLAP &&
                 drumlab_patch_of(TSEL, 36)->kind == DLAB_KICK);
    {
        char label[12];
        uint32_t count = 0;
        for (uint32_t p = 0; p < NPAGES; p++)
            if (PAGES[p].fam == FAM_EDIT && PAGES[p].title &&
                (str_eq(PAGES[p].title, "LEVELS") || str_eq(PAGES[p].title, "LEVELS 2") ||
                 str_eq(PAGES[p].title, "LEVELS 3")))
                count += page_visible(p);
        bad += check("Stage 25A: DRUM-LAB displays all three LEVELS pages", count == 3u);
        drumlab_pad_label(2u, drumlab_patch_of(TSEL, 38)->kind, label);
        bad += check("Stage 25A: pad number and selected type label", str_eq(label, "03 CLP"));
        go_title("LEVELS"); frame();
        TSEL->p[P_LN2] = 127;
        turn(EN_K3, -1); frame();
        bad += check("Stage 25A: LEVELS edits independently persisted pad levels",
                     TSEL->p[P_LN2] < 127 && TSEL->p[P_LN0] == 127);
    }
    apply_preset(1);
    bad += check("loading another DRUM-LAB kit resets its per-pad edits",
                 !drumlab_kit_dirty[0] && !drumlab_kit_ready[0]);
#if FELUCCA_NO_SEQUENCER
    {
        const uint32_t play_bit = 1u << panel.btn[B_PLAY];
        const uint32_t seq_bit = 1u << panel.btn[B_SEQ];
        /* A physical press is held across repeated input scans. The UI must
         * not clear the debounced state or retrigger the page toggle. */
        fm1_in.buttons |= play_bit;
        host_pressed |= play_bit;
        frame();
        bad += check("Stage 29A: PLAY held opens once and preserves physical key state",
                     ui.drum_perf_open && (fm1_in.buttons & play_bit));
        for (uint32_t i = 0; i < 12u; i++) frame();
        bad += check("Stage 29A: PLAY stays open across held-button scans",
                     ui.drum_perf_open && !ui.drum_map_open && (fm1_in.buttons & play_bit));
        fm1_in.buttons &= ~play_bit;
        frame();
        bad += check("Stage 29A: PLAY release does not close the page", ui.drum_perf_open);
        press(B_PLAY); /* second physical tap closes it */
        bad += check("Stage 29A: another PLAY tap closes the page", !ui.drum_perf_open);

        fm1_in.buttons |= seq_bit;
        host_pressed |= seq_bit;
        frame();
        bad += check("Stage 29A: SEQ held opens once and preserves physical key state",
                     ui.drum_map_open && (fm1_in.buttons & seq_bit));
        for (uint32_t i = 0; i < 12u; i++) frame();
        bad += check("Stage 29A: SEQ stays open across held-button scans",
                     ui.drum_map_open && !ui.drum_perf_open && (fm1_in.buttons & seq_bit));
        fm1_in.buttons &= ~seq_bit;
        frame();
        bad += check("Stage 29A: SEQ release does not close the page", ui.drum_map_open);
        press(B_SEQ);
        bad += check("Stage 29A: another SEQ tap closes the page", !ui.drum_map_open);
    }
    press(B_PLAY);
    bad += check("Stage 25B: PLAY opens per-pad performance screen", ui.drum_perf_open && !ui.drum_map_open);
    {
        uint32_t drawn = host_big_fills;
        for (uint32_t i = 0; i < 20u; i++) frame();
        bad += check("Stage 29B: idle PLAY screen does not blank/repaint the LCD", host_big_fills == drawn);
        turn(EN_K2, 1);
        bad += check("Stage 29B: PLAY knob edit repaints once", host_big_fills == drawn + 1u);
        drum_perf_sound[0][0] = 0u;
        ui.force = 1u; frame();
    }
    turn(EN_K1, 1); turn(EN_K2, 1); turn(EN_K3, 1); turn(EN_K4, 1);
    bad += check("Stage 25B: four knobs update selected drum pad only",
                 drum_perf_curve[0][0] == DPERF_SOFT && drum_perf_sound[0][0] == 1u &&
                 drum_perf_choke[0][0] == 1u && drum_retrigger[0][0] == 1u &&
                 !drum_perf_curve[0][1] && !drum_retrigger[0][1]);
    press(B_SEQ);
    bad += check("Stage 25B: SEQ switches to MIDI map, closes PLAY",
                 ui.drum_map_open && !ui.drum_perf_open);
    {
        uint32_t drawn = host_big_fills;
        for (uint32_t i = 0; i < 20u; i++) frame();
        bad += check("Stage 29B: idle SEQ screen does not blank/repaint the LCD", host_big_fills == drawn);
        turn(EN_K2, 1);
        bad += check("Stage 29B: SEQ knob edit repaints once", host_big_fills == drawn + 1u);
        drum_pad_pan[0][0] = 0;
        ui.force = 1u; frame();
    }
    press(B_PLAY);
    bad += check("Stage 25B: PLAY switches back to performance controls",
                 ui.drum_perf_open && !ui.drum_map_open);
    /* The middle canvas is capped at 150 rows: the uncovered part must
     * still be black, never the previous screen's pixels. */
    host_screen[190u * 240u + 237u] = swap16(0xFFFFu);
    ui.force = 1u; ui_draw();
    bad += check("Stage 25C: PLAY clears the entire screen",
                 host_screen[190u * 240u + 237u] == swap16(T_BG));
    press(B_ENV);
    bad += check("Stage 25C: ENV tap leaves PLAY immediately",
                 !ui.drum_perf_open && !ui.drum_map_open && !ui.home && cur_page()->fam == FAM_ENV);
    press(B_SEQ);
    bad += check("Stage 25C: SEQ opens from ENV", ui.drum_map_open);
    host_screen[190u * 240u + 237u] = swap16(0xFFFFu);
    ui.force = 1u; ui_draw();
    bad += check("Stage 25C: SEQ clears the entire screen",
                 host_screen[190u * 240u + 237u] == swap16(T_BG));
    press(B_EDIT);
    bad += check("Stage 25C: EDIT tap leaves SEQ immediately",
                 !ui.drum_perf_open && !ui.drum_map_open && !ui.home && cur_page()->fam == FAM_EDIT);
    press(B_PLAY);
    press(B_HOME);
    bad += check("Stage 25C: HOME tap leaves PLAY immediately",
                 !ui.drum_perf_open && !ui.drum_map_open && ui.home);
    press(B_PLAY);
#endif
    printf("DRUMLAB UI TESTS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return !!bad;
}
