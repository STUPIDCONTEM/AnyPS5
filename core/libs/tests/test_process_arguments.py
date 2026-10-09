import subprocess
import sys

arguments = ["plain", "with space", "", 'quote"inside', "trailing\\", "\u96ea", "a;b", "*", "\U0001f3ae"]
for values in ([], arguments):
    result = subprocess.run([sys.argv[1], *values], check=False, timeout=10)
    if result.returncode:
        raise SystemExit(result.returncode)
