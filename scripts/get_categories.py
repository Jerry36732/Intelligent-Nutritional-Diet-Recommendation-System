# -*- coding: utf-8 -*-
import re
import urllib.request

BASE = "https://nlc.chinanutri.cn/fq/"
req = urllib.request.Request(BASE, headers={"User-Agent": "Mozilla/5.0"})
html = urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")

# foodlist_0_1_0_0_0_1.htm pattern
pairs = set()
for m in re.findall(r"foodlist_\d+_(\d+)_(\d+)_\d+_\d+_1\.htm", html):
    pairs.add((int(m[0]), int(m[1])))

print("category pairs:", len(pairs))
for p in sorted(pairs):
    print(p)

# also categoryOne=0 for all?
pairs_all = set()
for m in re.findall(r"foodlist_\d+_(\d+)_(\d+)_", html):
    pairs_all.add((int(m[0]), int(m[1])))
print("all pairs from broader:", len(pairs_all))
