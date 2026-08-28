# -*- coding: utf-8 -*-
import urllib.request
import json

BASE = "https://nlc.chinanutri.cn/fq/"

# Try POST to API
import urllib.parse

url = BASE + "FoodInfoQueryAction!queryFlipPage.do"
data = urllib.parse.urlencode({
    "categoryOne": 1,
    "categoryTwo": 0,
    "foodName": "",
    "field": 0,
    "sort": 1,
    "pageNum": 1,
}).encode()

req = urllib.request.Request(
    url,
    data=data,
    headers={
        "User-Agent": "Mozilla/5.0",
        "Content-Type": "application/x-www-form-urlencoded",
        "Referer": BASE,
    },
    method="POST",
)
resp = urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")
print(resp[:2000])
print("---len---", len(resp))

# try json parse
try:
    j = json.loads(resp)
    print("keys:", j.keys() if isinstance(j, dict) else type(j))
    if isinstance(j, dict):
        for k, v in j.items():
            print(k, type(v), str(v)[:200])
except Exception as e:
    print("json err", e)
