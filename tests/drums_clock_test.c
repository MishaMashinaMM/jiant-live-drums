/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 18B live-only MIDI transport/clock checks. Host UI simulation supplies
 * the actual firmware handlers without requiring the FM-1 toolchain. */
#define FELUCCA_NO_SEQUENCER 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void clock_setup(void)
{
    ui_power_on();
    memset(&midi_clock, 0, sizeof midi_clock);
    fm1_ms = 1000u;
    song.g[G_CLOCK] = 1;  /* external MIDI clock */
    midi_clock.mode = 1;
}

static int live_clock_transport_test(void)
{
    int bad = 0;
    clock_setup();
    midi_clock_transport(0xFAu, fm1_ms);
    bad += check("DRUMS: MIDI START starts the live clock, not song playback",
                 midi_clock.live_running && !song.playing && !song.rec);
    midi_clock.pos = 1234u;
    midi_clock.rendered = 1234u;
    beat_pos = 31u;
    beat_n = 2u;
    bar_n = 3u;
    midi_clock_transport(0xFBu, fm1_ms + 10u);
    bad += check("DRUMS: MIDI CONTINUE preserves live clock position",
                 midi_clock.live_running && midi_clock.pos == 1234u && !song.playing);
    midi_clock_transport(0xFAu, fm1_ms + 20u);
    bad += check("DRUMS: MIDI START resets live clock position",
                 midi_clock.live_running && !midi_clock.pos && !midi_clock.rendered &&
                 !beat_pos && !beat_n && !bar_n);
    midi_clock_transport(0xFCu, fm1_ms + 30u);
    bad += check("DRUMS: MIDI STOP stops live clock without song transport",
                 !midi_clock.live_running && !song.playing);
    return bad;
}

static int live_clock_arp_test(void)
{
    int bad = 0;
    clock_setup();
    track_t *t = &trk[0];
    t->p[P_AMODE] = AM_UP;
    t->p[P_APROB] = 127;
    song.g[G_ROUTE] = 1;
    midi_route_update();
    midi_event(0x90u, 0, 60, 100);
    midi_clock_transport(0xFAu, fm1_ms);
    for (uint32_t i = 0; i < 48u; i++) {
        fm1_ms = 1000u + i * 20u;
        midi_clock_pulse(fm1_ms);
        events_block(CTL);
    }
    bad += check("DRUMS: MIDI clock advances the arpeggiator while song stays stopped",
                 midi_clock.live_running && t->arp_idx > 0u && !song.playing);
    midi_clock_transport(0xFCu, fm1_ms);
    bad += check("DRUMS: MIDI STOP releases sounding arpeggiator notes", !t->arp_note);

    clock_setup();
    t = &trk[0];
    t->p[P_AMODE] = AM_UP;
    t->p[P_APROB] = 127;
    song.g[G_ROUTE] = 1;
    midi_route_update();
    midi_event(0x90u, 0, 60, 100);
    midi_clock_transport(0xFAu, fm1_ms);
    midi_clock_pulse(fm1_ms);
    fm1_ms += 501u;
    events_block(CTL);
    bad += check("DRUMS: lost MIDI clock stops live timing and clears an arp note",
                 !midi_clock.live_running && !t->arp_note && !song.playing);
    return bad;
}

static int live_internal_arp_test(void)
{
    int bad = 0;
    clock_setup();
    song.g[G_CLOCK] = 0;
    midi_clock.mode = 0;
    song.g[G_ROUTE] = 1;
    midi_route_update();
    track_t *t = &trk[0];
    t->p[P_AMODE] = AM_UP;
    t->p[P_APROB] = 127;
    midi_event(0x90u, 0, 60, 100);
    for (uint32_t i = 0; i < 320u; i++)
        events_block(CTL);
    bad += check("DRUMS: internal clock advances live arpeggiator without song playback",
                 t->arp_idx > 0u && !song.playing && !transport_req);
    return bad;
}

static int live_only_ui_test(void)
{
    int bad = 0;
    clock_setup();
    song.g[G_CLOCK] = 0;
    press(B_SEQ);
    hold(B_SEQ);
    press(B_REC);
    hold(B_REC);
    press(B_PLAY);
    events_block(CTL);
    bad += check("DRUMS: SEQ/PLAY open drum pages without starting transport",
                 ui.drum_perf_open && !ui.drum_map_open && ui.layer == LAYER_NONE &&
                 !song.playing && !song.rec && !transport_req);
    ui_leds();
    int led_ok = 1;
    static const uint8_t OFF_BTN[] = {B_SEQ, B_REC, B_PLAY};
    for (uint32_t b = 0; b < sizeof OFF_BTN / sizeof OFF_BTN[0]; b++) {
        uint32_t pos = panel.btn[OFF_BTN[b]], idx = pos >> 3u, bit = 1u << (pos & 7u);
        led_ok &= !(fm1_led[idx] & bit) && !(fm1_led_dim[idx] & bit) &&
                  !(fm1_led_mid[idx] & bit) && !(fm1_led_hi[idx] & bit) &&
                  !(fm1_led_breath[idx] & bit);
    }
    bad += check("DRUMS: inactive transport buttons remain dark in every LED tier", led_ok);
    {
        int arrange_visible = 0;
        for (uint32_t i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == FAM_SAVE && PAGES[i].graph == GR_SONG && page_visible(i))
                arrange_visible = 1;
        bad += check("DRUMS: SAVE ARRANGE is hidden without deleting project data", !arrange_visible);
    }
    press(B_SAVE); /* ordinary page navigation closes the live PLAY screen */
    bad += check("DRUMS: SAVE navigation closes the live PLAY screen",
                 !ui.drum_map_open && !ui.drum_perf_open);
    go_title("TOOLS");
    ui.act = 0;
    turn(EN_K1, 1);
    int first = ui.act == 2u;
    turn(EN_K1, 1);
    int second = ui.act == 5u;
    turn(EN_K1, 1);
    int third = ui.act == 6u;
    turn(EN_K1, 1);
    bad += check("DRUMS: TOOLS offers INIT SOUND, NEW PROJECT, CPU TEST only",
                 first && second && third && ui.act == 6u && str_eq(act_name(5u), "RUN"));
    uint32_t old_mode = live.mode;
    uint32_t old_rec = live.srec;
    song_key(SK_MODE);
    song_key(SK_REC);
    song_key(SK_PAGE);
    bad += check("DRUMS: SAVE layer cannot open song mode, recorder or ARRANGE",
                 live.mode == old_mode && live.srec == old_rec && cur_page()->graph == GR_TOOLS);
    lys.nqc = 0;
    song_key(SK_PLAY);
    bad += check("DRUMS: SAVE layer section recall does not assemble quick chains", !lys.nqc && !chain.armed);
    return bad;
}

static int live_only_project_test(void)
{
    int bad = 0;
    ui_power_on();
    trk[0].step[2].note[0] = 60;
    trk[0].step[2].n = 1;
    trk[0].p[P_FCUT] = 73;
    project_t original, restored;
    project_store_t packed;
    project_capture(&original);
    int packed_ok = proj_pack(&packed, &original);
    int restored_ok = packed_ok && proj_import(&restored, &packed, sizeof packed);
    bad += check("DRUMS: old project sound and sequencer payload still round-trip",
                 restored_ok && restored.t[0].p[P_FCUT] == 73 &&
                 restored.t[0].step[2].note[0] == 60);
    if (restored_ok) {
        trk[0].p[P_FCUT] = 15;
        bad += check("DRUMS: loading an old project restores sound without playing its steps",
                     !project_restore_runtime(&restored) && trk[0].p[P_FCUT] == 73 &&
                     trk[0].step[2].note[0] == 60 && !song.playing);
    }
    return bad;
}

static int live_section_recall_test(void)
{
    int bad = 0;
    ui_power_on();
    trk[0].p[P_FCUT] = 81;
    section_store(0);
    int stored = project_used(0);
    trk[0].p[P_FCUT] = 22;
    int recalled = !section_jump(0);
    bad += check("DRUMS: SAVE sections still store and recall live sounds, not sequences",
                 stored && recalled && trk[0].p[P_FCUT] == 81 &&
                 !song.playing && !chain.running && !chain.armed);
    return bad;
}

static int live_only_song_editor_test(void)
{
    int bad = 0;
    ui_power_on();
    chain_config.count = 1;
    chain_config.row[0].slot = 0;
    chain_config.row[0].bars = 2;
    bad += check("DRUMS: song start requests (including editor PLAY) cannot arm transport",
                 chain_prepare() == 2u && !chain.armed && !chain.running && !transport_req &&
                 chain_config.count == 1 && chain_config.row[0].bars == 2);
    return bad;
}

int main(void)
{
    int bad = live_clock_transport_test() + live_clock_arp_test() + live_internal_arp_test() +
              live_only_ui_test() + live_only_project_test() + live_only_song_editor_test() +
              live_section_recall_test();
    printf("DRUMS CLOCK TESTS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return bad ? 1 : 0;
}
