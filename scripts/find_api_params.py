# -*- coding: utf-8 -*-
import re
import urllib.request

BASE = "https://nlc.chinanutri.cn/fq/"

def fetch(path):
    url = BASE + path
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    return urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")

for fname in ["scripts/compare.js", "scripts/front.js"]:
    html = fetch(fname)
    for m in re.finditer(r"queryFlipPage", html):
        start = max(0, m.start() - 500)
        end = min(len(html), m.end() + 1500)
        snippet = html[start:end]
        with open(f"c:/Users/ROG/Documents/System/scripts/snippet_{fname.replace('/','_')}.txt", "w", encoding="utf-8") as f:
            f.write(snippet)

# also inline from foodlist page
html = fetch("foodlist_0_1_0_0_0_1.htm")
idx = html.find("queryFoodInfoList")
with open("c:/Users/ROG/Documents/System/scripts/snippet_inline.txt", "w", encoding="utf-8") as f:
    f.write(html[idx:idx+3000])
