"""Verify primary-UV reference sampling through public Player material inputs."""
import argparse,json
from pathlib import Path
import test_material_coverage_gpu as coverage
import test_material_output_precision_gpu as numeric

def apply_policy(folder):
    for path in folder.rglob('*.gltf'):
        model=json.loads(path.read_text(encoding='utf-8'))
        for material in model.get('materials',[]):
            profile=material.get('extras',{}).get('azureRenderMaterial')
            if not profile:continue
            profile['schemaVersion']=2
            # The generic fixture uses lit emission to expose coverage without
            # borrowing the separate unlit-surface contract.
            features=profile.setdefault('features',[])
            if 'unlit-surface' in features:
                features.remove('unlit-surface');profile.pop('surface',None)
                features.append('emissive-mask');material['emissiveFactor']=[1,1,1]
            features.append('mirrored-coarse-uv')
        path.write_text(json.dumps(model,indent=2)+'\n',encoding='utf-8')

def run(player,output,validation,section="all",selected=None):
    original_coverage=coverage.prepare;original_numeric=numeric.prepare
    def covered(folder,*args,**kwargs):
        project=original_coverage(folder,*args,**kwargs);apply_policy(folder);return project
    def solid(folder,*args,**kwargs):
        values=original_numeric(folder,*args,**kwargs);apply_policy(folder);return values
    try:
        if section in ['all','coverage']:
            coverage.prepare=covered
            coverage.run(player,output/'coverage',validation,selected or ['generic-red-aa0','hair-red-aa0','hair-red-aa4','hair-shell-red-aa0','shadow','object-shadow'])
        if section in ['all','solid']:
            numeric.prepare=solid
            numeric.run(player,output/'solid',False,None if validation=='any' else validation)
    finally:coverage.prepare=original_coverage;numeric.prepare=original_numeric

if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--executable',required=True,type=Path);parser.add_argument('--output',required=True,type=Path);parser.add_argument('--expected-validation',choices=['any','enabled','disabled'],default='any');parser.add_argument('--section',choices=['all','coverage','solid'],default='all');parser.add_argument('--case',action='append');args=parser.parse_args()
    run(args.executable.resolve(),args.output.resolve(),args.expected_validation,args.section,args.case)
