from pathlib import Path
p=Path('src/assets/model/ModelFormatAdapter.cpp');s=p.read_text(encoding='utf-8');start=s.index('    // Embed external textures');end=s.index('    report.losses=',start)
block=s[start:end].replace('scene->','scene.').replace('io->','io.').replace('if(path.length&&path.C_Str()[0]==\'*\')continue;','if(!path.length||path.C_Str()[0]==\'*\')continue;')
helper='void embedTextures(aiScene& scene,SourceIO& io) {\n'+block+'}\n'
s=s[:start]+'    embedTextures(*scene,*io);\n'+s[end:];at=s.index('AssetTransferReport prepareModel(');s=s[:at]+helper+s[at:]
temp=p.with_suffix('.g21tmp');temp.write_text(s,encoding='utf-8');temp.replace(p);print('Extracted shared bounded texture embedding for import and export')
