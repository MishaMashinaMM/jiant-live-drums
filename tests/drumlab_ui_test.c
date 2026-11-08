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
    apply_preset(1);
    bad += check("loading another DRUM-LAB kit resets its per-pad edits",
                 !drumlab_kit_dirty[0] && !drumlab_kit_ready[0]);
    printf("DRUMLAB UI TESTS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return !!bad;
}
