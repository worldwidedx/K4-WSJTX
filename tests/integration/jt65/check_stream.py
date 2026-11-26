import json
from pathlib import Path
import subprocess
import sys


decoder, harness, recording, work_dir = sys.argv[1:]
work = Path(work_dir)
work.mkdir(parents=True, exist_ok=True)
result = subprocess.run(
    [sys.executable, harness, decoder, recording, "--mode", "JT65",
     "--depth-level", "1", "--nfa", "1300", "--nfb", "1700", "--quiet"],
    cwd=work, capture_output=True, text=True, timeout=60,
)
(work / "stdout.log").write_text(result.stdout)
(work / "stderr.log").write_text(result.stderr)
if result.returncode:
    raise SystemExit(f"JT65 stream failed: {result.stdout}\n{result.stderr}")
events = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
if any(event.get("t") == "error" for event in events):
    raise SystemExit(f"JT65 stream reported an error: {events}")
if not any(event.get("t") == "decode" and event.get("mode") == "JT65"
           and event.get("message", "").strip() == "K1ABC W9XYZ FN42" for event in events):
    raise SystemExit(f"JT65 stream did not decode the supplied recording: {events}")
if sum(event.get("t") == "decode_finished" for event in events) != 1:
    raise SystemExit(f"JT65 stream must complete the reception exactly once: {events}")
print("JT65 streaming PCM decode and completion passed")
