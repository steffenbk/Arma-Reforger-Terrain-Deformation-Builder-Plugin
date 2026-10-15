import json,urllib.request,sys
req={"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"decompile","arguments":{"addr":sys.argv[1]}}}
r=urllib.request.urlopen(urllib.request.Request("http://127.0.0.1:13337/mcp",json.dumps(req).encode(),{"Content-Type":"application/json","Accept":"application/json, text/event-stream"}),timeout=120).read().decode()
j=json.loads(r[r.index('{'):r.rindex('}')+1])
print(json.loads(j['result']['content'][0]['text'])['code'])
