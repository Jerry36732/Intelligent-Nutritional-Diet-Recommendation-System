# -*- coding: utf-8 -*-
import json
import urllib.parse
import urllib.request

BASE = "https://nlc.chinanutri.cn/fq/"

def post_api(params):
    url = BASE + "FoodInfoQueryAction!queryFoodInfoList.do"
    data = urllib.parse.urlencode(params).encode()
    req = urllib.request.Request(
        url, data=data,
        headers={"User-Agent": "Mozilla/5.0", "Content-Type": "application/x-www-form-urlencoded", "Referer": BASE},
        method="POST",
    )
    return json.loads(urllib.request.urlopen(req, timeout=30).read().decode("utf-8"))

j = post_api({"categoryOne": 1, "categoryTwo": 0, "foodName": "", "pageNum": 1, "field": 0, "flag": 1})
for i, group in enumerate(j["list"]):
    print(f"group {i} len={len(group)}:", group[:15])
