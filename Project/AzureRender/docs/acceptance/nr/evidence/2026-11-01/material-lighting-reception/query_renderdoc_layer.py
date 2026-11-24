import json,sys,traceback
from pathlib import Path
import renderdoc as rd
run=Path('D:/Projects/FYP/Project/AzureRender/build/runs/20261101-120125-character-environment-reception-fa70a7');result=dict(status='running',version=rd.GetVersionString(),documentation=rd.NeedVulkanLayerRegistration.__doc__)
try:
 info=rd.VulkanLayerRegistrationInfo();value=rd.NeedVulkanLayerRegistration(info);result['returnType']=type(value).__name__;result['value']=str(value)
 values=[info]
 for item in values:
  if hasattr(item,'flags'):result.update(flags=int(item.flags),flagNames=str(item.flags),myJSONs=list(item.myJSONs),otherJSONs=list(item.otherJSONs))
 result['status']='queried'
except Exception:result.update(status='failed',error=traceback.format_exc())
(run/'renderdoc-layer-query.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());sys.exit(0)
