#!/usr/bin/env python3
"""Exercise GUI startup rejection with isolated decoder substitutes on macOS."""

import argparse
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--signer", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[3]
    root = args.output.resolve() / str(os.getpid())
    root.mkdir(parents=True)
    cases = {
        "missing": ("", "within five seconds"),
        "version": ("<DecoderReady> version=2", "incompatible or unexpected"),
        "malformed": ("<DecoderReady> version=3 extra", "malformed or out-of-order"),
    }
    for name, (reply, expected) in cases.items():
        directory = root / name
        bundle = directory / "wsjtx.app"
        shutil.copytree(args.bundle.resolve(), bundle, symlinks=True)
        decoder = bundle / "Contents/MacOS/jt9"
        decoder.unlink()
        decoder.write_text("#!/bin/sh\n" + (f"echo '{reply}'\n" if reply else "")
                           + "exec /bin/sleep 20\n")
        decoder.chmod(0o755)
        if args.signer:
            with (directory / "sign.log").open("w") as log:
                subprocess.run([str(args.signer), str(bundle)], check=True,
                               stdout=log, stderr=subprocess.STDOUT)
        command = [str(bundle / "Contents/MacOS/wsjtx"), "--live-audio-test",
                   str(source / "samples/FT8/210703_133430.wav"),
                   "--live-audio-expected",
                   str(source / "test/fixtures/ft8_210703_133430_mtd.expected.txt"),
                   "--live-audio-data-dir", str(source),
                   "--rig-name", f"IPC-{name}-{os.getpid()}"]
        environment = dict(os.environ)
        environment["WSJT_QMAP_SHARED_MEMORY_KEY"] = f"ipc-{name}-{os.getpid()}"
        started = time.monotonic()
        with (directory / "output.log").open("w+") as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                       env=environment, start_new_session=True)
            try:
                code = process.wait(timeout=18)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                raise RuntimeError(f"{name}: GUI did not fail promptly; see {log.name}")
            elapsed = time.monotonic() - started
            log.seek(0)
            output = log.read()
        if code == 0 or expected not in output:
            raise RuntimeError(f"{name}: unexpected exit {code}; see {directory / 'output.log'}")
        if name == "missing" and elapsed < 4.5:
            raise RuntimeError("Missing handshake failed before the handshake deadline")
        print(f"{name}: PASS, exit={code}, elapsed={elapsed:.2f}s, log={directory / 'output.log'}",
              flush=True)


if __name__ == "__main__":
    main()
