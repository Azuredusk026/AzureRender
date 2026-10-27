"""Create a level snapshot with source ECS specular scales and a binding receipt."""
import argparse
import hashlib
import json
from pathlib import Path

from endfield_native.light_profiles import bind_specular_profiles,bind_directional_radiometry


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('level','profiles','output','receipt'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--source-scene',required=True)
    parser.add_argument('--profile-color-encoding',choices=['authored-srgb','linear'],
                        help='Explicit profile RGB encoding; temperature-enabled colors remain unresolved in the receipt')
    parser.add_argument('--main-header',type=Path,help='Caller-confirmed six-row CB3 header; radiometry only')
    parser.add_argument('--main-light',help='Explicit directional target identity; existing local direction retained')
    parser.add_argument('--main-enabled',choices=['0','1'],help='Caller-confirmed captured main-light activation')
    args=parser.parse_args()
    main_options=[args.main_header is not None,args.main_light is not None,args.main_enabled is not None]
    if any(main_options) and not all(main_options):raise ValueError('Main light header, identity and activation are required together')
    paths=[args.output.resolve(),args.receipt.resolve()]
    if paths[0]==paths[1] or any(p.exists() for p in paths):
        raise ValueError('Light profile output and receipt require distinct new files')
    level_raw=args.level.read_bytes();profile_raw=args.profiles.read_bytes()
    result,binding=bind_specular_profiles(json.loads(level_raw),json.loads(profile_raw),source_scene=args.source_scene,color_encoding=args.profile_color_encoding)
    main_binding=None
    if all(main_options):
        main_raw=args.main_header.read_bytes()
        result,main_binding=bind_directional_radiometry(result,main_raw,light_id=args.main_light,enabled=args.main_enabled=='1')
        main_binding['source']=dict(path=str(args.main_header.resolve()),sha256=hashlib.sha256(main_raw).hexdigest())
    encoded=(json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
    receipt=dict(schemaVersion=1,level=dict(path=str(args.level.resolve()),sha256=hashlib.sha256(level_raw).hexdigest()),
        profiles=dict(path=str(args.profiles.resolve()),sha256=hashlib.sha256(profile_raw).hexdigest()),
        binding=binding,output=dict(path=str(paths[0]),sha256=hashlib.sha256(encoded).hexdigest()))
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
    summary=dict(output=str(paths[0]),boundLights=binding['boundLights'],changedLights=binding['changedLights'],unmappedLights=binding['unmappedLights'])
    if args.profile_color_encoding:summary.update(coloredLights=binding['coloredLights'],unresolvedColorLights=binding['unresolvedColorLights'])
    print(json.dumps(summary))


if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError) as error:raise SystemExit(str(error))
