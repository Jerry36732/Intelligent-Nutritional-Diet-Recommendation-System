# -*- coding: utf-8 -*-
import re
import urllib.request

url = "https://nlc.chinanutri.cn/fq/"
req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
html = urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")
print("len", len(html))
links = re.findall(r'href=["\']([^"\']+)["\']', html)
for l in links:
    if any(x in l for x in ["fq", "food", "detail", "category", "list", "ajax", "api"]):
        print(l)
# script src
scripts = re.findall(r'src=["\']([^"\']+)["\']', html)
for s in scripts:
    print("script:", s)
# onclick or data
for m in re.findall(r"onclick=[\"']([^\"']+)[\"']", html):
    print("onclick:", m[:100])
