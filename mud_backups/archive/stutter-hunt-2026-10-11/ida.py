import json,urllib.request,sys
def call(name,args):
    req={"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":name,"arguments":args}}
    r=urllib.request.urlopen(urllib.request.Request("http://127.0.0.1:13337/mcp",json.dumps(req).encode(),{"Content-Type":"application/json","Accept":"application/json, text/event-stream"}),timeout=60).read().decode()
    return r
out=call("decompile",{"addr":sys.argv[1]})
i=out.find('{'); d=json.loads(out[i:]) if out.startswith('{') else None
print(out[:200000])
