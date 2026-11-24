"""Prepare an empty exploration level with the public authoring library."""
import argparse
import json
from pathlib import Path
import shutil

SOURCE=Path(__file__).resolve().parents[1]/'assets_public/exploration'

def prepare_empty(game):
    game=Path(game).resolve()
    if game.exists() and any(game.iterdir()):raise ValueError('Tutorial directory must be empty')
    assets=game/'assets';assets.mkdir(parents=True,exist_ok=True)
    project=json.loads((SOURCE/'project.azureproject').read_text(encoding='utf-8'))
    project['name']='Editor Authored Exploration'
    (game/'project.azureproject').write_text(json.dumps(project,indent=2),encoding='utf-8')
    empty=dict(schemaVersion=1,id='editor-exploration',sceneType='character',resources=[],nodes=[],renderSettings=dict(platform=False))
    (assets/'exploration.azurelevel').write_text(json.dumps(empty),encoding='utf-8')
    for path in (SOURCE/'assets').iterdir():
        if path.suffix in ('.lua','.json','.rml','.txt'):shutil.copy2(path,assets/path.name)
    shutil.copytree(SOURCE/'assets/prefabs',assets/'prefabs')
    for path in (assets/'prefabs').glob('*.azureprefab'):
        value=json.loads(path.read_text(encoding='utf-8'))
        for resource in value['resources']:
            resource['asset']='engine:/assets_public/exploration/assets/'+resource['asset'].split(':/',1)[1]
        path.write_text(json.dumps(value),encoding='utf-8')
    return game

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();print(prepare_empty(args.output))
