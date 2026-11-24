"""Produce a register-level evidence mirror for the selected DXBC kernel."""
from pathlib import Path
import re,struct,json,sys
precise='--precise' in sys.argv
trace_mode='--trace' in sys.argv
r=Path(__file__).parent
assembly=(r/'event-8702.asm').read_text()
instructions=[(int(m[1]),m[2]) for line in assembly.splitlines() if (m:=re.match(r'\s*(\d+):\s+(.*)',line))]
data=(r/'event-8702.dxbc').read_bytes();count=struct.unpack_from('<I',data,28)[0]
chunks=[]
for offset in struct.unpack_from('<'+'I'*count,data,32):
    if data[offset:offset+4] in (b'SHEX',b'SHDR'):
        size=struct.unpack_from('<I',data,offset+4)[0];chunks=list(struct.unpack_from('<'+'I'*(size//4),data,offset+8))
assert chunks and chunks[1]==len(chunks)
binary=[];cursor=2
while cursor<len(chunks):
    length=(chunks[cursor]>>24)&127
    assert length>0
    binary.append(chunks[cursor:cursor+length]);cursor+=length
binary=binary[-len(instructions):]
assert len(binary)==len(instructions) and [i for i,_ in instructions]==list(range(len(instructions)))
def split(text):
    parts=[];start=depth=0
    for i,c in enumerate(text):
        if c in '([':depth+=1
        elif c in ')]':depth-=1
        elif c==',' and depth==0:parts.append(text[start:i].strip());start=i+1
    parts.append(text[start:].strip());return parts

lines=['cbuffer C0:register(b0){float4 cb0[65];};','cbuffer C1:register(b1){float4 cb1[3];};','cbuffer C2:register(b2){float4 cb2[6];};',
       'StructuredBuffer<uint> t0:register(t0);',*[f'Texture2D<float4> t{i}:register(t{i});' for i in range(1,7)],
       'SamplerState s0:register(s0);','RWTexture2D<float4> u0:register(u0);',
       'RWTexture2D<unorm float4> u1:register(u1);','RWTexture2D<unorm float4> u2:register(u2);',
       '[numthreads(8,8,1)] void main(uint3 group:SV_GroupID,uint3 thread:SV_GroupThreadID){',
       'uint4 '+','.join(f'r{i}' for i in range(14))+';',
       'uint packedTile=t0[group.x];uint2 outputPixel=uint2(packedTile>>16,packedTile&65535u)*8+thread.xy;']
metadata=[]
for (number,text),tokens in zip(instructions,binary):
    match=re.match(r'(ld_structured_indexable\(structured_buffer, stride=4\)\(mixed,mixed,mixed,mixed\)|\S+)\s*(.*)',text)
    opcode,rest=match.groups();operands=split(rest) if rest else []
    literals=[]
    for i,token in enumerate(tokens):
        if token in (0x4001,0x4002):
            n=1 if token==0x4001 else 4;assert i+n<len(tokens)
            values=tokens[i+1:i+1+n];literals.append(values*4 if n==1 else values)
    literal_index=0
    def expression(value,domain='float'):
        global literal_index
        value=value.strip()
        if value.startswith('-'):return '-('+expression(value[1:],domain)+')'
        if value.startswith('abs('):return 'abs('+expression(value[4:-1],domain)+')'
        if value.startswith('l('):
            bits=literals[literal_index];literal_index+=1
            raw='uint4('+','.join('0x{:08x}u'.format(x) for x in bits)+')'
            return raw if domain=='uint' else ('asint('+raw+')' if domain=='int' else 'asfloat('+raw+')')
        builtin=re.fullmatch(r'(vThreadGroupID|vThreadIDInGroup)\.([xyzw]+)',value)
        if builtin:
            name='group' if builtin[1]=='vThreadGroupID' else 'thread';swizzle=builtin[2]
            swizzle=(swizzle*4)[:4] if len(swizzle)==1 else swizzle
            raw='uint4('+name+',0).'+swizzle
            return raw if domain=='uint' else ('asint('+raw+')' if domain=='int' else 'asfloat('+raw+')')
        value=re.sub(r'\.(x|y|z|w)$',lambda m:'.'+m[1]*4,value)
        if re.match(r'r\d+\.',value):
            return 'asfloat('+value+')' if domain=='float' else value if domain=='uint' else 'asint('+value+')'
        return value if domain=='float' else ('asuint('+value+')' if domain=='uint' else 'asint('+value+')')
    def raw_value(value):
        if value.startswith('-'):return '('+raw_value(value[1:])+')^0x80000000u'
        if value.startswith('abs('):return '('+raw_value(value[4:-1])+')&0x7fffffffu'
        return expression(value,'uint')
    def assign(destination,result,raw=False):
        if destination=='null':return
        name,mask=destination.split('.')
        lines.append('{}{}4 v{}={};{}={}.{};'.format('precise ' if precise and not raw else '', 'uint' if raw else 'float',number,result,destination,'v'+str(number) if raw else 'asuint(v'+str(number)+')',mask))
        if trace_mode:
            lines.append('if(all(outputPixel==uint2(9,0))){')
            for component,lane in enumerate(mask):
                bits=name+'.'+lane
                lines.append('u1[uint2({}, {})]=float4(float(({})&65535u)/65535.0,float(({})>>16)/65535.0,0,0);'.format(number,component,bits,bits))
            lines.append('}')
    lines.append('// {}: {}'.format(number,text))
    plain=opcode.removesuffix('_sat')
    if opcode in ['if_nz','if_z']:
        x=expression(operands[0],'uint');lines.append('if(({}).x{}0u){{'.format(x,'!=' if opcode=='if_nz' else '=='))
    elif opcode=='else':lines.append('}else{')
    elif opcode in ['endif','endloop']:lines.append('}')
    elif opcode=='loop':lines.append('[loop]while(true){')
    elif opcode in ['break','continue']:lines.append(opcode+';')
    elif opcode=='ret':lines.append('return;')
    elif opcode.startswith('sample_l'):
        dest,uv,resource,sampler,lod=operands
        u=expression(uv);level=expression(lod);tex,swizzle=resource.split('.')
        assign(dest,'{}.SampleLevel({},({}).xy,({}).x).{}'.format(tex,sampler,u,level,swizzle))
    elif opcode.startswith('ld_structured_indexable'):
        dest,index,offset,resource=operands;idx=expression(index,'uint');off=expression(offset,'uint')
        assign(dest,'uint4(t0[({}).x+({}).x/4u],0u,0u,0u)'.format(idx,off),True)
    elif opcode.startswith('ld_uav_typed'):
        dest,coordinate,resource=operands;coords=expression(coordinate,'uint');image,swizzle=resource.split('.')
        assign(dest,'{}[({}).xy].{}'.format(image,coords,swizzle))
    elif opcode.startswith('ld_indexable'):
        dest,coordinate,resource=operands;coords=expression(coordinate,'int');image,swizzle=resource.split('.')
        assign(dest,'{}.Load(({}).xyz).{}'.format(image,coords,swizzle))
    elif opcode=='store_uav_typed':
        image,coordinate,value=operands;coords=expression(coordinate,'uint');v=expression(value)
        assert coordinate=='r0.xyyy'
        if not trace_mode:lines.append('{}[outputPixel]={};'.format(image.split('.')[0],v))
    elif opcode=='sincos':
        # Both destinations are scalar in this frozen kernel.
        x=expression(operands[2]);lines.append('float sn{},cs{};sincos(({}).x,sn{},cs{});'.format(number,number,x,number,number))
        lines.append('{}=asuint(sn{});{}=asuint(cs{});'.format(operands[0],number,operands[1],number))
    else:
        dest=operands[0];values=operands[1:];result=None
        integer=plain in ['and','or','ushr','uge','iadd','imad']
        domain='int' if plain in ['iadd','imad','itof'] else 'uint' if integer or plain=='utof' else 'float'
        if plain in ['mov','movc']:
            v=[raw_value(x) for x in values]
        else:v=[expression(x,domain) for x in values]
        raw=integer or plain in ['mov','movc','ftou','lt','ge']
        if plain=='mov':result=v[0]
        elif plain=='movc':
            result='(({})!=uint4(0,0,0,0)?{}:{})'.format(*v)
        elif plain in ['add','mul','div']:result='({}){}({})'.format(v[0],{'add':'+','mul':'*','div':'/'}[plain],v[1])
        elif plain=='mad':result='mad({},{},{})'.format(*v)
        elif plain in ['lt','ge','uge']:
            result='uint4(({}){}({}))*0xffffffffu'.format(v[0],'<' if plain=='lt' else '>=',v[1])
        elif plain in ['min','max']:result='{}({},{})'.format(plain,*v)
        elif plain.startswith('dp'):
            n=int(plain[2]);mask='xyzw'[:n];result='float4(dot(({}).{},({}).{}),dot(({}).{},({}).{}),dot(({}).{},({}).{}),dot(({}).{},({}).{}))'.format(*([v[0],mask,v[1],mask]*4))
        elif plain in ['sqrt','rsq','frc','round_pi','round_z','exp','log']:
            function={'rsq':'rsqrt','frc':'frac','round_pi':'ceil','round_z':'trunc','exp':'exp2','log':'log2'}.get(plain,plain)
            result='{}({})'.format(function,v[0])
        elif plain in ['ftou','utof','itof']:
            result='uint4({})'.format(v[0]) if plain=='ftou' else 'float4({})'.format(v[0])
        elif plain in ['and','or','ushr']:result='({}){}({})'.format(v[0],{'and':'&','or':'|','ushr':'>>'}[plain],v[1])
        elif plain in ['iadd','imad']:
            result='asuint(({})+({}))'.format(*v) if plain=='iadd' else 'asuint(({})*({})+({}))'.format(*v)
        else:raise ValueError('Unsupported opcode '+opcode)
        if plain=='mov' and values[0].startswith('-'):
            raw=False;result=expression(values[0])
        if opcode.endswith('_sat'):result='saturate('+result+')'
        assign(dest,result,raw)
    assert literal_index==len(literals),(number,text,literal_index,literals)
    metadata.append(dict(instruction=number,opcode=opcode,binaryOpcode=tokens[0]&2047,immediateBits=literals))
lines.append('}')
(r/'ray-mirror.hlsl').write_text('\n'.join(lines)+'\n')
(r/'mirror-literals.json').write_text(json.dumps(metadata,indent=2))
print(json.dumps(dict(instructions=len(instructions),binaryInstructions=len(binary),hlslLines=len(lines))))
