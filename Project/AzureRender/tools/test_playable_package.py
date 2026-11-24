"""Publish the full standard quest, hide its inputs, move it and play it."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import build_game
from run_playable_long_run import isolated_environment
from test_level_cycles import prepare, verify
from test_playable_player import run as play


def execute(command,cwd,env=None,timeout=300):
    info=subprocess.STARTUPINFO();info.dwFlags|=subprocess.STARTF_USESHOWWINDOW;info.wShowWindow=0
    result=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,
        encoding='utf-8',errors='replace',timeout=timeout,startupinfo=info)
    assert result.returncode==0,result.stdout+result.stderr
    return result.stdout+result.stderr


def run(build_dir,editor,output,evidence):
    output=output.resolve();evidence=evidence.resolve();evidence.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='playable-input-',dir=evidence) as temporary:
        root=Path(temporary);engine=root/'Release engine';project=root/'Standard exploration'
        install=execute(['cmake','--install',build_dir.resolve(),'--config','Release','--prefix',engine],root)
        (evidence/'install.log').write_text(install,encoding='utf-8')
        task=prepare(project)
        shutil.copy2(task,evidence/'cycles.input.json')
        actions=[dict(frame=1,command='build',install=str(engine),output=str(output),replace=output.exists()),dict(frame=2,command='wait-build')]
        (root/'publish.json').write_text(json.dumps(actions),encoding='utf-8')
        report=evidence/'editor.json'
        log=execute([editor.resolve(),'--editor-close-policy', 'save', '--editor-project',project/'project.azureproject','--editor-actions',root/'publish.json',
            '--fixed-frame-step','--smoke-frames','8','--runtime-report',report],root)
        (evidence/'editor.log').write_text(log,encoding='utf-8')
        data=json.loads(report.read_text(encoding='utf-8'));assert data['gameBuildPassed'] and all(row['passed'] for row in data['editorActions'])
        assert 'VUID-' not in log and 'Validation Error' not in log and 'Allocator after unload: buffers=0 images=0' in log
        manifest=build_game.verify_package(output)
        assert all('AzureRender.exe' not in item['path'] and 'AzureMetaGen.exe' not in item['path'] for item in manifest['files'])
        assert (output/'GAME-GUIDE.md').is_file() and (output/'QUALITY.json').is_file()
        quality=json.loads((output/'QUALITY.json').read_text(encoding='utf-8'))
        assert quality['entities']==100 and quality['animatedCharacters']==4 and quality['pointLights']==8
        env=isolated_environment();moved=root/'Moved exploration with spaces'
        shutil.move(output,moved);project.rename(root/'Project unavailable');engine.rename(root/'Engine unavailable')
        try:
            # This starts through the distributed entry before the long input route.
            entry=execute([env['COMSPEC'],'/d','/c','start-game.cmd','--smoke-frames','4','--fixed-frame-step'],moved,env)
            (evidence/'entry.log').write_text(entry,encoding='utf-8')
            assert 'Allocator after unload: buffers=0 images=0' in entry
            route=play(moved/'bin/AzurePlayer.exe',evidence/'quest',project=moved/'game/project.azureproject',env=env,cwd=moved)
            # Each invocation owns fresh timing files; the Player's overwrite
            # guard remains active while completed reports stay in evidence.
            timing=root/'cycles.json';runtime=root/'cycles.runtime.json'
            log=execute([moved/'bin/AzurePlayer.exe','--project',moved/'game/project.azureproject',
                '--width','1920','--height','1080','--game-actions',evidence/'cycles.input.json',
                '--fixed-frame-step','--smoke-frames','6600','--gpu-timing','--gpu-timing-output',timing,'--runtime-report',runtime],moved,env)
            (evidence/'cycles.log').write_text(log,encoding='utf-8')
            for source in (timing, timing.with_suffix('.csv'), runtime):
                shutil.copy2(source,evidence/source.name)
            assert 'VUID-' not in log and 'Validation Error' not in log and 'Allocator after unload: buffers=0 images=0' in log
            cycle=verify(timing);(evidence/'cycles.summary.json').write_text(json.dumps(cycle,indent=2)+'\n',encoding='utf-8')
            actual=json.loads(timing.read_text(encoding='utf-8'));assert Path(actual['asset']).is_relative_to(moved)
            assert actual['sceneWidth']==1920 and actual['sceneHeight']==1080 and actual['sceneTriangles']>0
            game=json.loads(runtime.read_text(encoding='utf-8'));assert not game['scriptErrors'] and not game['presentationErrors']
            build_game.verify_package(moved)
            immutable=moved/'GAME-GUIDE.md';saved=immutable.read_bytes();immutable.write_bytes(saved+b'\nchanged\n')
            try:
                try:build_game.verify_package(moved)
                except ValueError:pass
                else:raise AssertionError('Changed operation guide was accepted')
            finally:immutable.write_bytes(saved)
        finally:shutil.move(moved,output)
        build_game.verify_package(output)
        summary=dict(status='passed',package=str(output),files=len(manifest['files']),licenses=len(list((output/'share/AzureRender/licenses').glob('*.txt'))),
            movedDirectory=True,sourceUnavailable=True,engineUnavailable=True,isolatedPath=env['PATH'],
            tamperRejected=True,completeQuest=True,quest=json.loads((evidence/'quest/summary.json').read_text()),
            cycles=cycle,quality=quality,packageManifestHash=build_game.digest(output/'game_manifest.json'))
        (evidence/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(summary),flush=True)
        return summary


if __name__=='__main__':
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('--build-dir',type=Path,required=True)
    parser.add_argument('--editor',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--evidence',type=Path,required=True);args=parser.parse_args()
    run(args.build_dir,args.editor,args.output,args.evidence)
