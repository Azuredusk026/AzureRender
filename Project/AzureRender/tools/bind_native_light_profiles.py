"""Create an explicit-source light snapshot and provenance receipt."""
import argparse
import hashlib
import json
from pathlib import Path

from endfield_native.light_profiles import bind_specular_profiles,bind_directional_radiometry,bind_captured_lights


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('level','output','receipt'):
        parser.add_argument('--'+name,type=Path,required=True)
    source=parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--profiles',type=Path)
    source.add_argument('--captured-constants',type=Path)
    parser.add_argument('--selection',type=Path,help='Versioned captured indices, masks, origin and replacement identities')
    parser.add_argument('--source-scene',required=True)
    parser.add_argument('--profile-color-encoding',choices=['authored-srgb','linear'],
                        help='Explicit profile RGB encoding; temperature-enabled colors remain unresolved in the receipt')
    parser.add_argument('--main-header',type=Path,help='Caller-confirmed six-row CB3 header; radiometry only')
    parser.add_argument('--main-light',help='Explicit directional target identity; existing local direction retained')
    parser.add_argument('--main-enabled',choices=['0','1'],help='Caller-confirmed captured main-light activation')
    args=parser.parse_args()
    if bool(args.captured_constants)!=bool(args.selection) or (args.captured_constants and args.profile_color_encoding):
        raise ValueError('Captured constants require their selection; profile color encoding belongs to ECS profiles')
    main_options=[args.main_header is not None,args.main_light is not None,args.main_enabled is not None]
    if any(main_options) and not all(main_options):raise ValueError('Main light header, identity and activation are required together')
    paths=[args.output.resolve(),args.receipt.resolve()]
    if paths[0]==paths[1] or any(p.exists() for p in paths):
        raise ValueError('Light profile output and receipt require distinct new files')
    level_raw=args.level.read_bytes()
    describe=lambda path,data:dict(path=str(path.resolve()),sha256=hashlib.sha256(data).hexdigest())
    if args.profiles:
        profile_raw=args.profiles.read_bytes()
        result,binding=bind_specular_profiles(json.loads(level_raw),json.loads(profile_raw),source_scene=args.source_scene,color_encoding=args.profile_color_encoding)
        source_documents=dict(profiles=describe(args.profiles,profile_raw))
    else:
        constant_raw=args.captured_constants.read_bytes();selection_raw=args.selection.read_bytes()
        result,binding=bind_captured_lights(json.loads(level_raw),constant_raw,json.loads(selection_raw),source_scene=args.source_scene)
        source_documents=dict(capturedConstants=describe(args.captured_constants,constant_raw),selection=describe(args.selection,selection_raw))
    main_binding=None
    if all(main_options):
        main_raw=args.main_header.read_bytes()
        result,main_binding=bind_directional_radiometry(result,main_raw,light_id=args.main_light,enabled=args.main_enabled=='1')
        main_binding['source']=dict(path=str(args.main_header.resolve()),sha256=hashlib.sha256(main_raw).hexdigest())
    encoded=(json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
    receipt=dict(schemaVersion=1 if args.profiles else 2,level=describe(args.level,level_raw),
        **source_documents,binding=binding,output=dict(path=str(paths[0]),sha256=hashlib.sha256(encoded).hexdigest()))
    if main_binding is not None:receipt['mainBinding']=main_binding
    for p in paths:p.parent.mkdir(parents=True,exist_ok=True)
    created=[]
    try:
        for path,data in [(paths[0],encoded),(paths[1],(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))]:
            with path.open('xb') as stream:
                created.append(path);stream.write(data)
    except Exception:
        for path in created:path.unlink()
        raise
    summary=dict(output=str(paths[0]))
    if args.profiles:summary.update(boundLights=binding['boundLights'],changedLights=binding['changedLights'],unmappedLights=binding['unmappedLights'])
    else:summary.update(createdLights=binding['createdLights'],replacedLights=binding['replacedLights'],indices=binding['indices'])
    if args.profile_color_encoding:summary.update(coloredLights=binding['coloredLights'],unresolvedColorLights=binding['unresolvedColorLights'])
    print(json.dumps(summary))


if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError) as error:raise SystemExit(str(error))
