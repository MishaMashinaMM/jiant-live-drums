# Jiant Live Drums: experimental macro-control device test

This installer is built from an experimental MIDI-macro branch, **not** the published
`main` firmware. It is intended for a single FM1 test, not a public release.
The installer bundles the exact `.fwsc` produced by its GitHub Actions run.

## Before connecting the FM1

1. Back up all sounds, projects, banks and settings **to a file on your PC**.
   With the currently working firmware, use the [JLD editor](https://mishamashinamm.github.io/jiant-live-drums/webapp/editor/),
   connect the FM1, then choose **Complete backup → Save backup**. Keep that
   JSON file somewhere safe. Do not rely only on project slots in flash.
2. Make sure you have a working way back to the published JLD firmware:
   the [public JLD installer](https://mishamashinamm.github.io/jiant-live-drums/webapp/installer/).
   A backup is **not** itself a firmware recovery image; bootloader failures
   can still require a hardware recovery procedure.
3. Use a direct USB data cable and Chrome or Edge on your computer. Close DAWs,
   MIDI monitors, editors and any other apps using the FM1 MIDI port.

## Launch the isolated installer

1. Extract **all** files from `jld-macro-control-local-installer-*.zip`
   into a new folder. Do not mix these files with the public installer.
2. In Windows File Explorer, open that folder (it contains `index.html`,
   `firmware/` and `webapp/`). Type `powershell` into Explorer's address bar.
3. In PowerShell run `py -m http.server 8000` (if port 8000 is already in
   use, stop the earlier emulator server with Ctrl+C first).
4. Open `http://localhost:8000/webapp/installer/` in Chrome or Edge.
   **Check that the version contains `macro` and `preview`**, not the public
   main release, before connecting or installing.
5. Confirm the ZIP contains `firmware/PACKAGE-SHA256SUMS` and
   `firmware/SOURCE_COMMIT`. The package checksum can be independently verified
   in PowerShell with `Get-FileHash .\firmware\felucca-*.fwsc -Algorithm SHA256`.

## Flash and verify (only when ready)

1. Connect the FM1, follow the installer's device detection and preflight
   instructions, then review the device and firmware identity before choosing
   **Install**. Keep the USB connection and power uninterrupted.
2. After the installer reports success, unplug/reboot as instructed. Verify
   that JLD boots and audio still works before changing any saved projects.
3. Check REC macro creation, CC control, ON/OFF, scrolling to all 32 rows and delete
   confirmation with a connected external MIDI controller.
4. Save a test project with nondefault GLO > MIXER MIDI RX channels and two
   different REC macros; reboot and reload to check persistence.
5. Test a different sound engine on the assigned track: enabled macro should
   show `S` (suspended) and return to `Y` when its original engine is restored.
6. Test both USB MIDI and the TRS input if available; note any dropped notes,
   stuck notes or unexpected parameter changes.

## If anything goes wrong

- **Do not repeat an installation blindly.** Copy the installer's diagnostic
  report and record the device display / LED state.
- Do not restore a backup until the firmware has booted and the editor has
  confirmed a compatible connection.
- If the FM1 still boots, the public JLD installer is the intended route back
  to the known tested `main` firmware. A device stuck in its bootloader may
  need the separately documented hardware recovery process.

No production site files are changed by this local package.
