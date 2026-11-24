import json,struct
from pathlib import Path
run=Path(__file__).resolve().parent
paths=list((run/'candidate-project').rglob('hero-restored-inputs-v8.glb'));assert len(paths)==1
data=paths[0].read_bytes();size=struct.unpack_from('<I',data,12)[0];gltf=json.loads(data[20:20+size]);result=[]
for i in [1,2,3,4,5]:
    m=gltf['materials'][i];e=m.get('extras',{})
    result.append(dict(index=i,name=m['name'],alphaMode=m.get('alphaMode','OPAQUE'),doubleSided=m.get('doubleSided',False),extrasKeys=list(e),profile=e.get('azureRenderMaterial'),other={k:v for k,v in e.items() if k!='azureRenderMaterial'}))
(run/'clothing-source-inspection.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
brief=[]
for row in result:
    source=row['other']['nativeCharacterProfileSource'];floats=source['floats']
    keys=['_EnableRealisticLighting','_DisableSelfShadow','_ShadowColorInput','_DiffuseColorInput','_NPRInput','_PBRInput','_UseShadowLutTex','_UseDiffRampMap','_DiffuseBrightness','_DiffuseSaturation','_DiffRampBrightness','_Specular','_ShadowColorBrightness','_ShadowColorSaturation']
    brief.append(dict(index=row['index'],alphaMode=row['alphaMode'],profile=row['profile'],sourceMaterial=source['material'],keywords=source['keywords'],floats={k:floats[k] for k in keys if k in floats}))
(run/'clothing-source-brief.json').write_bytes((json.dumps(brief,indent=2)+'\n').encode())
for row in brief:print(row['index'],row['alphaMode'],row['floats'],row['keywords'])
