"""Create a level snapshot with source ECS specular scales and a binding receipt."""
import argparse
import hashlib
import json
from pathlib import Path

from endfield_native.light_profiles import bind_specular_profiles


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('level','profiles','output','receipt'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--source-scene',required=True)
    args=parser.parse_args()
    paths=[args.output.resolve(),args.receipt.resolve()]
    if paths[0]==paths[1] or any(p.exists() for p in paths):
        raise ValueError('Light profile output and receipt require distinct new files')
    level_raw=args.level.read_bytes();profile_raw=args.profiles.read_bytes()
    result,binding=bind_specular_profiles(json.loads(level_raw),json.loads(profile_raw),source_scene=args.source_scene)
    encoded=(json.dumps(result,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
    receipt=dict(schemaVersion=1,level=dict(path=str(args.level.resolve()),sha256=hashlib.sha256(level_raw).hexdigest()),
        profiles=dict(path=str(args.profiles.resolve()),sha256=hashlib.sha256(profile_raw).hexdigest()),
        binding=binding,output=dict(path=str(paths[0]),sha256=hashlib.sha256(encoded).hexdigest()))
    for p in paths:p.parent.mkdir(parents=True,exist_ok=True)
    created=[]
    try:
        for path,data in [(paths[0],encoded),(paths[1],(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))]:
            with path.open('xb') as stream:
                created.append(path);stream.write(data)
    except Exception:
        for path in created:path.unlink()
        raise
    print(json.dumps(dict(output=str(paths[0]),boundLights=binding['boundLights'],changedLights=binding['changedLights'],unmappedLights=binding['unmappedLights'])))


if __name__=='__main__':
    try:main()
    except (ValueError,OSError,KeyError) as error:raise SystemExit(str(error))
