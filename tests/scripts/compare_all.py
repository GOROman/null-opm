#!/usr/bin/env python3 -I
"""Render every manifest input with vgm2wav and compare against the reference WAV.

usage: compare_all.py <build-dir> [--markdown]
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.dirname(ROOT)
REF = os.path.join(ROOT, "ref")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    build = os.path.abspath(sys.argv[1])
    vgm2wav = os.path.join(build, "tools", "vgm2wav")
    wavdiff = os.path.join(build, "tools", "wavdiff")
    with open(os.path.join(REF, "manifest.json")) as f:
        manifest = json.load(f)
    rows = []
    for e in manifest["entries"]:
        ref = os.path.join(REF, e["name"], "ref.wav")
        if not os.path.exists(ref):
            rows.append((e["name"], "missing", None))
            continue
        out = os.path.join(REF, e["name"], "nullopm.wav")
        cmd = [vgm2wav, os.path.join(PROJECT, e["input"]), out]
        if e.get("clock"):
            cmd += ["--clock", str(e["clock"])]
        if e.get("rate"):
            cmd += ["--rate", str(e["rate"])]
        subprocess.run(cmd, check=True, capture_output=True)
        dcmd = [wavdiff, "--json", out, ref]
        if e.get("kind") == "hardware":
            dcmd += ["--align", "2000", "--normalize"]
        r = subprocess.run(dcmd, capture_output=True, text=True)
        rows.append((e["name"], "ok", json.loads(r.stdout)))
    md = "--markdown" in sys.argv
    if md:
        print("| name | frames | exact % | max abs | rms | snr dB |")
        print("|---|---|---|---|---|---|")
    for name, status, st in rows:
        if st is None:
            print(f"| {name} | {status} | | | | |" if md else f"{name}: {status}")
        elif md:
            print(f"| {name} | {st['frames']} | {st['exact_ratio']*100:.3f} | {st['max_abs']:.0f} | "
                  f"{st['rms']:.3f} | {st['snr_db']:.1f} |")
        else:
            print(f"{name}: exact {st['exact_ratio']*100:.3f}% max {st['max_abs']:.0f} "
                  f"rms {st['rms']:.3f} snr {st['snr_db']:.1f} dB")


if __name__ == "__main__":
    main()
