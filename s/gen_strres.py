# gen_strres.py
import sys, io, re

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

with open("setup.rc", encoding="utf-8", errors="ignore") as f:
    for line in f:
        m = re.search(r'^(IDS[a-zA-Z0-9_]+)\s+(".*")', line.strip())
        if m:
            rid = m.group(1)
            outer = m.group(2)
            inner = outer[1:-1].replace('""', '\\"')
            value = '"' + inner + '"'
            print(f'{rid}, _T({value}),')