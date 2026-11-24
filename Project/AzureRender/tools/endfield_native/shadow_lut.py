"""Bind enabled native shadow color strips to the typed GPU lookup contract.

The output exposes the lookup surface quantity. Native NPR composition and
runtime character globals require their own validation.
"""
import argparse
import base64
import copy
import hashlib
from io import BytesIO
import json
from pathlib import Path

from PIL import Image


def identity(row):
    return row['source'],row['path_id']


def restore_shadow_luts(document,audit,sources,payloads,artifacts):
    result=copy.deepcopy(document)
    by_name={row['object']['name']:row for row in audit['materials']}
    if len(by_name)!=len(audit['materials']):raise ValueError('Ambiguous native material names')
    raw={identity(row['object']):row['data'] for row in sources if row.get('available',True)}
    image_sources={identity(row['object']):row for row in artifacts}
    report={'materials':[],'native_npr_composition_verified':False,
            'scope':'Native enabled lookup inputs; GPU lookup quantity is independent of full NPR composition'}
    for material in result.get('materials',[]):
        source=by_name.get(material.get('name',''))
        if source is None:continue
        source_data=raw[identity(source['object'])]
        if '_SHADOW_LUT_TEX' not in source_data.get('m_ValidKeywords',[]):continue
        values={row['name']:row['value'] for row in source['floats']}
        if values.get('_UseShadowLutTex')!=1:raise ValueError('Shadow LUT keyword and material toggle disagree')
        bindings=[row for row in source['textures'] if row['slot']=='_ShadowLutTex']
        if len(bindings)!=1:raise ValueError('Enabled shadow LUT requires one source texture binding')
        binding=bindings[0];ref=binding['reference']['resolved']
        if ref is None or binding['color_space']!=1 or binding['scale']!=[1,1] or binding['offset']!=[0,0]:
            raise ValueError('Shadow LUT requires an sRGB source and identity lookup transform')
        key=identity(ref)
        if key not in payloads or key not in image_sources:raise ValueError('Missing source shadow LUT image or metadata')
        metadata=image_sources[key]
        if type(metadata.get('vertical_flip')) is not bool:raise ValueError('Source LUT vertical orientation is required')
        payload=payloads[key]
        image=Image.open(BytesIO(payload))
        if image.height<2 or image.height>64 or image.width!=image.height**2:raise ValueError('Invalid shadow LUT strip dimensions')
        from endfield_native.character_profiles import material_class
        profile=material.setdefault('extras',{}).setdefault('azureRenderMaterial',
            {'schemaVersion':2,'class':material_class(material['name'])})
        if 'shadowColorLut' in profile:raise ValueError('Target shadow LUT is already configured')
        images=result.setdefault('images',[]);textures=result.setdefault('textures',[])
        images.append({'name':ref['name'],'uri':'data:image/png;base64,'+base64.b64encode(payload).decode('ascii')})
        index=len(textures);textures.append({'source':len(images)-1})
        profile['shadowColorLut']={'schemaVersion':1,'texture':index,'flipV':metadata['vertical_flip']}
        report['materials'].append({'name':material['name'],'source':source['object'],'source_keyword':'_SHADOW_LUT_TEX',
            'source_toggle':values['_UseShadowLutTex'],'source_texture':ref,'png_sha256':hashlib.sha256(payload).hexdigest(),
            'width':image.width,'height':image.height,'source_vertical_flip':metadata['vertical_flip'],
            'output_texture':index,'source_color_space':binding['color_space']})
    return result,report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for field in ('input','source-root','output','report'):parser.add_argument('--'+field,type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists() or args.report.exists() or args.output.resolve()==args.report.resolve():raise ValueError('Outputs must be distinct new files')
    from convert_material_semantics import read_asset,write_asset
    names=('source_audit.json','material_sources.json','source_texture_artifacts.json')
    packets={name:json.loads((args.source_root/name).read_text(encoding='utf8')) for name in names}
    payloads={}
    for row in packets[names[2]]:
        path=(args.source_root/row['png']['path']).resolve()
        if not path.is_relative_to(args.source_root.resolve()):raise ValueError('Source image escapes source root')
        data=path.read_bytes()
        if hashlib.sha256(data).hexdigest().lower()!=row['png']['sha256'].lower():raise ValueError('Source image digest mismatch')
        payloads[identity(row['object'])]=data
    document,tail=read_asset(args.input)
    output,report=restore_shadow_luts(document,packets[names[0]],packets[names[1]],payloads,packets[names[2]])
    args.output.parent.mkdir(parents=True,exist_ok=True);write_asset(args.output,output,tail)
    verified,verified_tail=read_asset(args.output)
    if verified!=output or verified_tail!=tail:raise AssertionError('Output lookup or existing binary changed')
    report.update(input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),output_sha256=hashlib.sha256(args.output.read_bytes()).hexdigest(),
        source_files={name:hashlib.sha256((args.source_root/name).read_bytes()).hexdigest() for name in names},
        generator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),binary_tail_preserved=True)
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2),encoding='utf8')
    print(json.dumps({'materials':len(report['materials']),'binary_tail_preserved':True}))


if __name__=='__main__':main()
