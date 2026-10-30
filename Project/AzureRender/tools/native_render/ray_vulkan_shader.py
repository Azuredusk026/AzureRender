"""Derive the frozen Vulkan reference from the fingerprinted source mirror."""
import hashlib
import re

SOURCE_SHA256='b72d73f2aadc5955290dc92652f8534eb464fe2a2485c9670318128d9a14a8f1'


def reference_source(data: bytes)->str:
    if hashlib.sha256(data).hexdigest()!=SOURCE_SHA256:
        raise ValueError('Native ray source fingerprint differs from its reference')
    source=data.decode('utf-8').replace('\r\n','\n').replace('\r','\n')
    for index,fmt in enumerate(('r11g11b10f','rg16','r8')):
        declaration=re.search(r'RWTexture2D<[^;]+ u'+str(index)+r':register\(u'+str(index)+r'\);',source)
        if declaration is None:raise ValueError('Missing storage declaration')
        source=source[:declaration.start()]+'[[vk::image_format("'+fmt+'")]] '+source[declaration.start():]
    source=source.replace('dot(','orderedDot(')
    helpers='''float orderedDot(float2 a,float2 b){precise float v=a.x*b.x;return mad(a.y,b.y,v);}
float orderedDot(float3 a,float3 b){precise float v=a.x*b.x;return mad(a.z,b.z,mad(a.y,b.y,v));}
float orderedDot(float4 a,float4 b){precise float v=a.x*b.x;return mad(a.w,b.w,mad(a.z,b.z,mad(a.y,b.y,v)));}
'''
    source=source.replace('[numthreads',helpers+'[numthreads',1)
    def replace(index,code):
        nonlocal source
        start=source.index('// '+str(index)+':');end=source.index('// '+str(index+1)+':')
        source=source[:start]+'// '+str(index)+': explicit projection order\n'+code+'\n'+source[end:]
    for index,register,denominator in ((254,9,'z'),(301,6,'z'),(305,3,'w')):
        target='r9.xy' if index==254 else 'r1.yz'
        replace(index,f'float2 q{index}=mad(asfloat(r{register}.xy),rcp(asfloat(r{register}.{denominator}))*0.5,0.5);{target}=asuint(q{index});')
        replace(index+1,'')
    return '\n'.join(line for line in source.splitlines() if line.strip())+'\n'
