"""Shared model request validation; contains no content-domain rules."""
def validate_chat(params):
    if set(params)!={'runId','prompt','schema','history','profile','timeoutMs'}:raise ValueError('Unknown or missing chat fields')
    for name,limit in [('runId',128),('profile',128),('prompt',2*1024*1024)]:
        value=params[name]
        if not isinstance(value,str) or not value or len(value.encode('utf-8'))>limit:raise ValueError('Invalid '+name)
    if type(params['timeoutMs']) is not int or not 1<=params['timeoutMs']<=300000:raise ValueError('Invalid deadline')
    import json
    if not isinstance(params['schema'],dict) or len(json.dumps(params['schema']).encode('utf-8'))>2*1024*1024:raise ValueError('Invalid schema')
    history=params['history']
    if not isinstance(history,list) or len(history)>24 or len(json.dumps(history,ensure_ascii=False).encode('utf-8'))>128*1024:raise ValueError('History exceeds budget')
    for message in history:
        if not isinstance(message,dict) or set(message)!={'role','content'} or message['role'] not in ('user','assistant') or not isinstance(message['content'],str):raise ValueError('Invalid history message')
