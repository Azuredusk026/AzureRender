"""Create original arm and door authoring fixtures using shared physics components."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path


def create(root: Path, mechanism: str):
    root.mkdir(parents=True, exist_ok=False)
    assets = root / 'assets'
    assets.mkdir()
    source = Path(__file__).resolve().parents[1] / 'assets_public/scene_inspector/assets/cube.gltf'
    shutil.copy2(source, assets / 'cube.gltf')
    def component(name, data, version=1):
        return dict(type=name, version=version, data=data)
    def node(identity, position, scale=(1, 1, 1), dynamic=False, geometry=True):
        return dict(id=identity, name=identity, resourceId='cube' if geometry else '', components={
            'azure.transform': component('azure.transform', dict(translation=position, scale=scale)),
            'azure.rigid-body': component('azure.rigid-body', dict(halfExtent=[1, 1, 1], dynamic=dynamic))})
    arm = mechanism == 'arm'
    fixed = node('fixed', [0, 3, 0], [.12, .12, .12])
    moving = node('moving', [1, 3, 0], [1, .12 if arm else 1, .12], True)
    axis = [0, 0, 1] if arm else [0, 1, 0]
    joint = dict(id='joint', name='Hinge', resourceId='', components={
        'azure.hinge': component('azure.hinge', dict(bodyA='fixed', bodyB='moving', anchorA=[0, 0, 0],
            anchorB=[-1, 0, 0], axis=axis, normal=[1, 0, 0], minimumAngle=-60 if arm else -15, maximumAngle=60 if arm else 100))})
    floor = node('ground', [0, -.2, 0], [5, .2, 5])
    level = dict(schemaVersion=1, id='physics-' + mechanism, sceneType='character', renderSettings=dict(platform=False),
                 resources=[dict(id='cube', asset='assets:/cube.gltf')], nodes=[fixed, moving, joint, floor], lights=[])
    project = dict(schemaVersion=1, id='physics-' + mechanism, name='Physics ' + mechanism,
        mounts=[dict(name='assets', path='assets')], startupScene='assets:/mechanism.azurelevel',
        runtime=dict(schemaVersion=1, input={}, systems=[dict(id='physics', config={})]))
    for path, data in [(root / 'project.azureproject', project), (assets / 'mechanism.azurelevel', level),
        (root / 'provenance.json', dict(generator='tools/create_physics_project.py', version=1, mechanism=mechanism,
            inputs=[dict(path='assets_public/scene_inspector/assets/cube.gltf', sha256=hashlib.sha256(source.read_bytes()).hexdigest())], license='MIT'))]:
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mechanism', choices=['arm', 'door'], required=True)
    args = parser.parse_args()
    create(args.output, args.mechanism)
