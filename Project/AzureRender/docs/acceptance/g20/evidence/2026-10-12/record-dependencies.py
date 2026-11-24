from pathlib import Path
import hashlib,json,subprocess
root=Path.cwd();out=root/'build/g20-dependency-evaluation-2026-10-12';vcpkg=Path('C:/Users/23587/Tools/vcpkg')
packages=[]
for name,version,port in [('libvorbis','1.3.7',4),('libogg','1.3.6',0),('libflac','1.5.0',0),('mp3lame','3.100',17)]:
    versions=json.loads((vcpkg/'versions'/f'{name[0]}-'/f'{name}.json').read_text(encoding='utf-8'))['versions']
    entry=next(v for v in versions if v.get('version',v.get('version-semver',v.get('version-string')))==version and v.get('port-version',0)==port)
    tree=entry['git-tree'];portfile=subprocess.check_output(['git','-C',str(vcpkg),'show',tree+':portfile.cmake']).decode()
    share=out/'vcpkg/x64-windows/share'/name
    packages.append({'name':name,'version':version,'portVersion':port,'portTree':tree,'portfile':portfile,'licenseSha256':hashlib.sha256((share/'copyright').read_bytes()).hexdigest(),'spdx':json.loads((share/'vcpkg.spdx.json').read_text(encoding='utf-8'))})
document={'schemaVersion':1,'packages':packages,'fixtureLicense':'CC0-1.0','fixtureSource':'tests/AudioCodecTests.cpp','runtime':'miniaudio with stb_vorbis backend','nativePaths':'Windows wide-character file API','lameRole':'Deterministic MP3 fixtures; output adapter belongs to G23'}
path=root/'docs/licenses/audio-codecs-source.json';temp=path.with_suffix('.tmp');temp.write_text(json.dumps(document,indent=2)+'\n',encoding='utf-8');temp.replace(path)
print('Recorded pinned ports, complete SPDX sources and license hashes')
