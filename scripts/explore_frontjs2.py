# -*- coding: utf-8 -*-
import urllib.request

BASE = "https://nlc.chinanutri.cn/fq/"

def fetch(path):
    url = BASE + path
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    return urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")

html = fetch("scripts/front.js")
idx = html.find("queryFoodInfoList")
print(html[idx:idx+2500])
