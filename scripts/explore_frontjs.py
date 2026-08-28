# -*- coding: utf-8 -*-
import re
import urllib.request

BASE = "https://nlc.chinanutri.cn/fq/"

def fetch(path):
    url = BASE + path
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    return urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")

html = fetch("scripts/front.js")
# find ajax urls
for m in re.findall(r'["\']([^"\']*(?:ajax|food|query|list)[^"\']*)["\']', html, re.I):
    if len(m) < 100:
        print(m)
print("---")
# url patterns
for m in re.findall(r'url\s*:\s*["\']([^"\']+)["\']', html):
    print("url:", m)
