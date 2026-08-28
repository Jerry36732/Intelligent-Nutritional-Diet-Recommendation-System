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

j = post({"categoryOne": 0, "categoryTwo": 0, "foodName": "", "pageNum": 170, "field": 0, "flag": 1})
print("page 170 items", len(j["list"]))
# full indices for first item
if j["list"]:
    for i, v in enumerate(j["list"][0]):
        print(i, v)
