#!/usr/bin/env python3 -I
"""Download and verify reference WAV files listed in tests/ref/manifest.json."""
import hashlib
import json
import os
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.path.join(ROOT, "ref")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    with open(os.path.join(REF, "manifest.json")) as f:
        manifest = json.load(f)
    ok = True
    for e in manifest["entries"]:
        d = os.path.join(REF, e["name"])
        os.makedirs(d, exist_ok=True)
        dst = os.path.join(d, "ref.wav")
        if not os.path.exists(dst):
            if not e.get("url"):
                print(f"skip {e['name']}: no url")
                continue
            print(f"fetch {e['name']} <- {e['url']}")
            urllib.request.urlretrieve(e["url"], dst)
        got = sha256(dst)
        if e.get("sha256") and got != e["sha256"]:
            print(f"FAIL {e['name']}: sha256 {got} != {e['sha256']}")
            ok = False
        else:
            print(f"ok   {e['name']}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
