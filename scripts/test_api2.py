# -*- coding: utf-8 -*-
import json
import urllib.parse
import urllib.request

BASE = "https://nlc.chinanutri.cn/fq/"

def post_api(endpoint, params):
    url = BASE + endpoint
    data = urllib.parse.urlencode(params).encode()
    req = urllib.request.Request(
        url,
        data=data,
        headers={
            "User-Agent": "Mozilla/5.0",
            "Content-Type": "application/x-www-form-urlencoded",
            "Referer": BASE,
            "X-Requested-With": "XMLHttpRequest",
        },
        method="POST",
    )
    return urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")

resp = post_api(
    "FoodInfoQueryAction!queryFoodInfoList.do",
    {
        "categoryOne": 1,
        "categoryTwo": 0,
        "foodName": "",
        "pageNum": 1,
        "field": 0,
        "flag": 1,
    },
)
print("resp len", len(resp))
j = json.loads(resp)
print("keys", j.keys())
print("totalPages", j.get("totalPages"), "currentPage", j.get("currentPage"))
lst = j.get("list", [])
print("list groups", len(lst))
if lst:
    print("first group len", len(lst[0]))
    print("first group", lst[0][:3])

# fetch detail page
if lst and lst[0]:
    fid = lst[0][0]
    detail_url = BASE + f"foodinfo/{fid}.html"
    req = urllib.request.Request(detail_url, headers={"User-Agent": "Mozilla/5.0"})
    detail = urllib.request.urlopen(req, timeout=30).read().decode("utf-8", errors="replace")
    with open("c:/Users/ROG/Documents/System/scripts/detail_sample.html", "w", encoding="utf-8") as f:
        f.write(detail)
    print("detail saved, id=", fid, "len=", len(detail))
