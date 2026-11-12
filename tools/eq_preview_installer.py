#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Local, isolated WebMIDI installer preview for feature/track-eq.

Usage (from the repository root):
    python3 tools/eq_preview_installer.py

Build the firmware first with bash build.sh. Then open:
    http://localhost:8765/webapp/installer/

This generates the *existing* validated installer for the local experimental
package, and serves it on localhost (a secure context for Web MIDI in Chrome
and Edge). Does not publish to Pages or change the stable site.
"""
from __future__ import annotations

import hashlib
import html
import http.server
import os
from pathlib import Path
import socketserver
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / "build/felucca.fwsc"
OUT = ROOT / "build/eq-preview-site"
PORT = 8765


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def main() -> None:
    branch = git("branch", "--show-current")
    if branch != "feature/track-eq":
        sys.exit(f"Refusing to create EQ preview outside feature/track-eq (current: {branch!r})")
    if not PKG.is_file():
        sys.exit("Missing build/felucca.fwsc. Run bash build.sh first.")
    sha = git("rev-parse", "--short=7", "HEAD")
    version = f"v0.3-eq-{sha}-preview"
    digest = hashlib.sha256(PKG.read_bytes()).hexdigest()
    # make_site.py checks the FM-1_9xx identity and FELUCCA-LOADER-1
    # marker before producing a site, and bundles the actual WebMIDI installer.
    subprocess.run(
        [sys.executable, str(ROOT / "web/make_site.py"), str(PKG),
         version, str(OUT)], cwd=ROOT, check=True
    )
    page = OUT / "webapp/installer/index.html"
    source = page.read_text(encoding="utf-8")
    warning = (
        '<div role="alert" style="border:3px solid #e36b39;'
        'background:#2d1913;color:#fff;padding:14px;margin:0 0 20px;'
        'font:600 15px/1.5 sans-serif">'
        'EXPERIMENTAL JLD v0.3 — TRACK EQ PREVIEW (NOT STABLE)'
        '<div style="font-weight:400">Local-only preview. Firmware has not '
        'been verified on physical FM1 hardware with EQ active. Back up '
        'projects, keep the stable firmware package and have a recovery method '
        'available before flashing. Device CPU headroom is not yet known. '
        'This preview does not replace the public Drums installer.</div>'
        f'<div style="font:12px monospace;overflow-wrap:anywhere">Branch: '
        f'{html.escape(branch)} · commit: {html.escape(sha)}'
        f'<br>Package SHA-256: {digest}</div></div>'
    )
    marker = '<main>'
    if source.count(marker) != 1:
        sys.exit("Installer layout changed: expected one <main>; refusing to publish preview.")
    page.write_text(source.replace(marker, marker + "\n" + warning, 1), encoding="utf-8")

    os.chdir(OUT)

    class Handler(http.server.SimpleHTTPRequestHandler):
        def end_headers(self) -> None:
            self.send_header("Cache-Control", "no-store")
            super().end_headers()

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True

    print("\nPREVIEW INSTALLER: http://localhost:8765/webapp/installer/", flush=True)
    print(f"Source commit: {sha} | package SHA-256: {digest}", flush=True)
    print("Open from Chrome/Edge on the same computer. Ctrl+C stops the server.", flush=True)
    print("No firmware is flashed by starting this server.\n", flush=True)
    try:
        with Server(("127.0.0.1", PORT), Handler) as server:
            server.serve_forever()
    except KeyboardInterrupt:
        print("\nPreview server stopped.")
    except OSError as exc:
        sys.exit(f"Cannot serve http://localhost:{PORT}: {exc}")


if __name__ == "__main__":
    main()
