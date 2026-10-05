"""Standard-library-only child used by C++ process lifetime/quoting tests."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

mode, destination, *arguments = sys.argv[1:]
if mode == "arguments":
    Path(destination).write_text(json.dumps(arguments, ensure_ascii=False), encoding="utf-8")
    print("x" * 300000, flush=True)  # Output larger than a pipe buffer must not deadlock the caller.
    print("stderr fixture", file=sys.stderr, flush=True)
elif mode == "failure":
    print("intentional child failure", file=sys.stderr, flush=True)
    sys.exit(7)
elif mode == "tree":
    child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
    Path(destination).write_text(json.dumps({"parent": os.getpid(), "child": child.pid}))
    time.sleep(60)
