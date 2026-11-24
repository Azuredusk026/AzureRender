"""Protocol fixture: initialize correctly, then stop consuming input."""
import json
import sys
import time
row=json.loads(sys.stdin.readline())
print(json.dumps(dict(jsonrpc='2.0',id=row['id'],result=dict(protocolVersion=1))),flush=True)
time.sleep(2)
