import json,urllib.request,sys,re
def call(name,args):
    req={"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":name,"arguments":args}}
    r=urllib.request.urlopen(urllib.request.Request("http://127.0.0.1:13337/mcp",json.dumps(req).encode(),{"Content-Type":"application/json","Accept":"application/json, text/event-stream"}),timeout=120).read().decode()
    j=json.loads(r[r.index('{'):r.rindex('}')+1]); return json.loads(j['result']['content'][0]['text'])
for g in sys.argv[1:]:
    x=call("xrefs_to",{"addrs":[g],"limit":50})
    fns=[e['fn']['addr'] for e in (x[0] if isinstance(x,list) else x['result'][0])['xrefs'] if e['fn'] and e['fn']['addr'].startswith('0x14012')]
    for f in set(fns):
        c=call("decompile",{"addr":f})['code']
        m=re.findall(r'"([^"]+)"',c); print(g,'->',m)
    if not fns: print(g,'-> no 0x14012 registration xref')
