# -*- coding: utf-8 -*-
import re
import urllib.request

BASE = "https://nlc.chinanutri.cn/fq/"

def fetch(path):
    url = BASE + path if not path.startswith("http") else path
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    return urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")

# sample list page
html = fetch("foodlist_0_1_0_0_0_1.htm")
print("=== LIST PAGE sample ===")
print(html[:3000])
links = re.findall(r'href=["\']([^"\']+)["\']', html)
for l in links:
    if "food" in l.lower() or "detail" in l.lower() or "info" in l.lower():
        print("link:", l)
