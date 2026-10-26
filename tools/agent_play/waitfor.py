#!/usr/bin/env python3
"""waitfor.py <field> <value> [timeout] [auto.json]: block until auto.json[field] == value (true/false/number/text; dotted paths allowed)."""
import json, re, sys, time, os
f, want, to = sys.argv[1], sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else 20
auto = sys.argv[4] if len(sys.argv) > 4 else 'build/play/auto.json'
t0 = time.time()
while time.time() - t0 < to:
    try:
        if os.path.getmtime(auto) >= t0:
            d = json.loads(re.sub(r'[\x00-\x1f]', ' ', open(auto).read())); v = d
            for k in f.split('.'): v = v[k]
            if str(v).lower() == want.lower(): print(f'{f}={v} after {time.time()-t0:.1f}s'); sys.exit(0)
    except Exception: pass
    time.sleep(0.05)
print(f'timeout {f}'); sys.exit(1)
