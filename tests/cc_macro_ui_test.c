/* SPDX-License-Identifier: GPL-3.0-only */
/* The REC editor is independent of the retired sequencer and MIDI LEARN. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0;
    ui_power_on();
    cm_count = cm_pick = cm_delete = cm_wait = cm_ui_open = 0;
    memset(cm_rows, 0, sizeof cm_rows);
    press(B_REC);
    bad += check("REC opens the independent project macro editor", cm_ui_open && !cm_count);
    press(B_OCTUP);
    bad += check("OCT+ starts CC and channel capture", cm_wait == 1u);
    midi_control(2u, 40u, 65u); frame();
    bad += check("Incoming CC captures channel 3 and returns to sound pages",
                 cm_wait == 2u && cm_heard_cc == 40u && cm_heard_ch == 2u && !cm_ui_open);
    cm_knob_cap(&trk[1], P_LEVEL);
    bad += check("A parameter knob completes a macro with default endpoints",
                 cm_wait == 0u && cm_ui_open && cm_count == 1u && cm_rows[0][0] == 40u &&
                 cm_rows[0][1] == 0x12u && cm_rows[0][2] == trk[1].eng_req &&
                 cm_rows[0][3] == P_LEVEL && cm_rows[0][4] == CM_ON &&
                 cm_rows[0][5] == 0u && cm_rows[0][6] == 127u);
    /* The same encoder may deliver more detents just after capture. They
     * belong to choosing a parameter, never to editing the new macro. */
    turn(EN_K1, 1); turn(EN_K2, 1); turn(EN_K4, 1);
    bad += check("Residual capture detents cannot change RNG, endpoints or REV",
                 cm_rows[0][4] == CM_ON && cm_rows[0][5] == 0u && cm_rows[0][6] == 127u);
    frames(240);
    turn(EN_K1, 1); turn(EN_K2, 3); turn(EN_K3, -4); turn(EN_K4, 1);
    bad += check("K1-K4 edit type, A/B endpoints and reverse",
                 (cm_rows[0][4] & (CM_ON | CM_TGL | CM_REV)) == (CM_ON | CM_TGL | CM_REV) &&
                 cm_rows[0][5] == 3u && cm_rows[0][6] == 123u);
    turn(EN_ALGO, -1);
    bad += check("ALGORITHM switches the macro off without deleting it",
                 !(cm_rows[0][4] & CM_ON) && cm_count == 1u);
    turn(EN_ALGO, 1);
    bad += check("ALGORITHM restores the saved macro", (cm_rows[0][4] & CM_ON) != 0u);
    press(B_OCTDN);
    bad += check("OCT- requests delete confirmation", cm_delete && cm_count == 1u);
    press(B_OCTDN);
    bad += check("OCT- cancels deletion", !cm_delete && cm_count == 1u);
    press(B_OCTDN); press(B_OCTUP);
    bad += check("OCT+ confirms deletion and clears the row", !cm_delete && !cm_count && !cm_rows[0][0]);
    press(B_OCTUP); press(B_REC);
    bad += check("REC cancels an unfinished CC capture", !cm_wait && !cm_ui_open);
    press(B_REC); press(B_OCTUP);
    midi_control(0u, 43u, 64u); frame();
    turn(EN_K1, 1);
    bad += check("Turning a real sound-page knob finishes CC capture and returns to REC",
                 cm_count == 1u && cm_rows[0][0] == 43u && cm_rows[0][1] == 0u &&
                 cm_rows[0][3] < P_COUNT && cm_ui_open && !cm_wait && cm_rows[0][4] == CM_ON);
    turn(EN_K1, 1); turn(EN_K2, 1);
    bad += check("Real K1/K2 clockwise capture cannot accidentally start TGL",
                 cm_rows[0][4] == CM_ON && cm_rows[0][5] == 0u && cm_rows[0][6] == 127u);
    frames(240);
    turn(EN_K1, 1);
    bad += check("K1 edits TGL normally once the capture gesture has settled",
                 (cm_rows[0][4] & CM_TGL) != 0u);
    return bad ? 1 : 0;
}
