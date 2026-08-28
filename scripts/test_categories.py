# -*- coding: utf-8 -*-
import json
import urllib.parse
import urllib.request

BASE = "https://nlc.chinanutri.cn/fq/"

def post(params):
    url = BASE + "FoodInfoQueryAction!queryFoodInfoList.do"
    data = urllib.parse.urlencode(params).encode()
    req = urllib.request.Request(url, data=data, headers={"User-Agent": "Mozilla/5.0", "Content-Type": "application/x-www-form-urlencoded", "Referer": BASE})
    return json.loads(urllib.request.urlopen(req, timeout=30).read().decode("utf-8"))

for c1, c2 in [(0,0), (1,0), (1,30)]:
    j = post({"categoryOne": c1, "categoryTwo": c2, "foodName": "", "pageNum": 1, "field": 0, "flag": 1})
    print(c1, c2, "pages", j["totalPages"], "items page1", len(j["list"]))
