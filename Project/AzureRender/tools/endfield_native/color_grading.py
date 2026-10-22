"""Source-bound grading candidates and captured strips for the public .azlut contract.

Uniforms are shader-ready values. Authoring-to-uniform conversion and the active
runtime values are deliberately recorded as unknown in every candidate receipt.
The implementation follows the recovered Vulkan fragment program's operation
order, including both color-space matrix stages and the curve texture samples.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import numpy as np

GENERATOR_VERSION = 1
LUMA_AP1 = np.array([.27222898602485657,.6740819811820984,.05368949845433235])
LUMA_SRGB = np.array([.21267290413379669,.71515220403671265,.07217500358819962])
LINEAR_TO_LMS = np.array([[.39040499925613403,.5499410033226013,.008926319889724255],
    [.07084160298109055,.9631720185279846,.0013577500358223915],
    [.02310819923877716,.1280210018157959,.9362450242042542]])
LMS_TO_LINEAR = np.array([[2.8584699630737305,-1.628790020942688,-.024891000241041184],
    [-.21018199622631073,1.1582000255584717,.00032428099075332284],
    [-.041811998933553696,-.11816900223493576,1.0686700344085693]])
LINEAR_TO_ACES = np.array([[.43970099091529846,.38297799229621887,.1773349940776825],
    [.08979229629039764,.8134229779243469,.09676159918308258],
    [.01754399947822094,.11154399812221527,.8707039952278137]])
ACES_TO_AP1 = np.array([[1.4514392614364624,-.2365107536315918,-.21492856740951538],
    [-.07655377686023712,1.17622971534729,-.09967592358589172],
    [.008316148072481155,-.006032449658960104,.9977163076400757]])
AP1_TO_OUTPUT = np.array([[1.70505154132843,-.6217907071113586,-.08325868099927902],
    [-.13025714457035065,1.1408028602600098,-.010548190213739872],
    [-.024003269150853157,-.12896877527236938,1.152971625328064]])


def neutral_candidate_parameters() -> dict:
    """An authored neutral hypothesis, expressed directly in native uniform space."""
    return {"_ColorBalance":[1,1,1],"_ColorFilter":[1,1,1],
        "_ChannelMixerRed":[1,0,0],"_ChannelMixerGreen":[0,1,0],"_ChannelMixerBlue":[0,0,1],
        "_HueSatCon":[0,1,1],"_Lift":[0,0,0],"_Gamma":[1,1,1],"_Gain":[1,1,1],
        "_Shadows":[1,1,1],"_Midtones":[1,1,1],"_Highlights":[1,1,1],
        "_ShaHiLimits":[0,.3,.55,1],"_SplitShadows":[.5,.5,.5,0],"_SplitHighlights":[.5,.5,.5]}


def neutral_candidate_curves() -> dict:
    # The native 1/256 sample offset suggests a 128-wide curve texture.
    # Width, texels and sampler address mode remain candidate values.
    identity=(np.arange(128,dtype=float)/128).tolist()
    constant=[.5]*128
    return {"master":identity,"red":identity.copy(),"green":identity.copy(),"blue":identity.copy(),
        "hue_vs_hue":constant,"hue_vs_sat":constant.copy(),"sat_vs_sat":constant.copy(),"lum_vs_sat":constant.copy()}


def _validate_parameters(parameters: dict, curves: dict) -> tuple[dict,dict]:
    expected=neutral_candidate_parameters()
    if set(parameters)!=set(expected):
        raise ValueError("Parameters must contain exactly the native shader uniform contract")
    converted={}
    for key,value in parameters.items():
        array=np.asarray(value,dtype=float)
        if array.shape!=(len(expected[key]),) or not np.isfinite(array).all() or np.max(np.abs(array))>65536:
            raise ValueError(f"Invalid native uniform {key}")
        converted[key]=array
    limits=converted["_ShaHiLimits"]
    if limits[0]>=limits[1] or limits[2]>=limits[3]:
        raise ValueError("Shadow and highlight smoothstep edges require increasing ranges")
    for name in ("_SplitShadows","_SplitHighlights"):
        if ((converted[name][:3]<0)|(converted[name][:3]>1)).any():
            raise ValueError("Split-tone color requires channels in [0,1]")
    if set(curves)!=set(neutral_candidate_curves()):
        raise ValueError("All eight native grading curves are required")
    converted_curves={}
    for key,value in curves.items():
        array=np.asarray(value,dtype=float)
        if array.ndim!=1 or not 2<=array.size<=4096 or not np.isfinite(array).all() or ((array<0)|(array>1)).any():
            raise ValueError(f"Invalid grading curve {key}")
        converted_curves[key]=array
    return converted,converted_curves


def decode_logc3(coordinate):
    coordinate=np.asarray(coordinate,dtype=float)
    if not np.isfinite(coordinate).all():
        raise ValueError("LogC lattice coordinates must be finite")
    return (np.power(10,(coordinate-.3860360085964203)*4.095658302307129)-.047995999455451965)*.17999999225139618


def _curve(curve, coordinate):
    # Normalized texture coordinates; linear filtering, clamp-to-edge.
    index=np.clip(np.asarray(coordinate)*len(curve)-.5,0,len(curve)-1)
    lower=np.floor(index).astype(int);upper=np.minimum(lower+1,len(curve)-1)
    fraction=index-lower
    return np.clip(curve[lower]*(1-fraction)+curve[upper]*fraction,0,1)


def _smoothstep(low,high,value):
    value=np.clip((value-low)/(high-low),0,1)
    return value*value*(3-2*value)


def _soft_light(color,blend):
    high=np.sqrt(color)*(2*blend-1)+2*color*(1-blend)
    low=2*color*blend+color*color*(1-2*blend)
    return np.where(blend>=.5,high,low)


def modified_aces_curve(value):
    value=np.asarray(value,dtype=float)
    reciprocal=np.clip(1/(value*(value*2.936044931411743+.8871219754219055)+.806888997554779),.0001,10000)
    return np.clip(value*(value*2.7850849628448486+.10777200013399124)*reciprocal,0,1)


def evaluate_native_chain(scene_linear,parameters=None,curves=None,*,return_trace=False):
    p,c=_validate_parameters(neutral_candidate_parameters() if parameters is None else parameters,
        neutral_candidate_curves() if curves is None else curves)
    scene_linear=np.asarray(scene_linear,dtype=float)
    if scene_linear.shape[-1:]!=(3,) or not np.isfinite(scene_linear).all():
        raise ValueError("Native color-chain input requires finite RGB values")
    trace={}
    with np.errstate(divide="raise",invalid="raise",over="raise"):
        balanced=((scene_linear@LINEAR_TO_LMS.T)*p["_ColorBalance"])@LMS_TO_LINEAR.T
        trace["white_balance"]=balanced
        ap1=(balanced@LINEAR_TO_ACES.T)@ACES_TO_AP1.T;trace["aces_ap1"]=ap1
        contrast=np.power(np.maximum(ap1@LUMA_AP1,9.999999974752427e-7)*5.55555534362793,p["_HueSatCon"][2]-1)
        gamma_color=np.power(np.abs(np.maximum(0,ap1*contrast[...,None]*p["_ColorFilter"])),.4545454680919647)
        trace["contrast_filter"]=gamma_color
        balance=np.clip(np.clip(gamma_color,0,1)@LUMA_AP1+p["_SplitShadows"][3],0,1)
        shadows=.5+(p["_SplitShadows"][:3]-.5)*(1-balance[...,None])
        highlights=.5+(p["_SplitHighlights"][:3]-.5)*balance[...,None]
        split=np.power(np.abs(_soft_light(_soft_light(gamma_color,shadows),highlights)),2.200000047683716)
        trace["split_tone"]=split
        mixer=np.stack([p["_ChannelMixerRed"],p["_ChannelMixerGreen"],p["_ChannelMixerBlue"]])
        mixed=split@mixer.T;trace["channel_mixer"]=mixed
        luminance=mixed@LUMA_AP1;limits=p["_ShaHiLimits"]
        sha=_smoothstep(limits[0],limits[1],luminance);hi=_smoothstep(limits[2],limits[3],luminance)
        tonal=mixed*(p["_Shadows"]*(1-sha[...,None])+p["_Midtones"]*(sha-hi)[...,None]+p["_Highlights"]*hi[...,None])
        trace["shadows_midtones_highlights"]=tonal
        lifted=tonal*p["_Gain"]+p["_Lift"]
        adjusted=np.sign(lifted)*np.power(np.abs(lifted),p["_Gamma"]);trace["lift_gamma_gain"]=adjusted
        # Direct transcription of the native branchless RGB-to-HSV calculation.
        red,green,blue=np.moveaxis(adjusted,-1,0)
        choose=green>=blue
        px=np.where(choose,green,blue);py=np.where(choose,blue,green)
        pz=np.where(choose,0.,-1.);pw=np.where(choose,-.3333333432674408,.6666666865348816)
        choose=red>=px
        qx=np.where(choose,red,px);qy=py;qz=np.where(choose,pz,pw);qw=np.where(choose,px,red)
        delta=qx-np.minimum(qw,qy)
        hue=np.abs(qz+(qw-qy)/(6*delta+9.999999747378752e-5))
        saturation=delta/(qx+9.999999747378752e-5)
        shifted=hue+p["_HueSatCon"][0]
        shifted+=_curve(c["hue_vs_hue"],shifted)-.5
        shifted=np.where(shifted<0,shifted+1,np.where(shifted>1,shifted-1,shifted))
        hue_rgb=np.clip(np.abs(((shifted[...,None]+[1.,.6666666865348816,.3333333432674408])%1)*6-3)-1,0,1)
        hsv_color=(1+(hue_rgb-1)*saturation[...,None])*qx[...,None]
        lum=(hsv_color@LUMA_AP1)[...,None]
        sat=p["_HueSatCon"][1]*(_curve(c["hue_vs_sat"],hue)*2)*(_curve(c["sat_vs_sat"],saturation)*2)*(_curve(c["lum_vs_sat"],adjusted@LUMA_SRGB)*2)
        hsv_color=lum+sat[...,None]*(hsv_color-lum);trace["hue_saturation"]=hsv_color
        compressed=hsv_color/(np.max(hsv_color,axis=-1)[...,None]+1)+.00390625
        master=_curve(c["master"],compressed)+.00390625
        channels=np.stack([_curve(c[name],master[...,axis]) for axis,name in enumerate(("red","green","blue"))],axis=-1)
        restored=np.maximum(0,channels/(1-np.max(channels,axis=-1)[...,None]));trace["master_rgb_curves"]=restored
        mapped=modified_aces_curve(restored);trace["aces_modified"]=mapped
        mapped_luma=(mapped@LUMA_AP1)[...,None]
        output=(mapped_luma+(mapped-mapped_luma)*.9300000071525574)@AP1_TO_OUTPUT.T
        trace["output_matrix"]=output
        normalized=np.clip(output/np.maximum(np.max(output,axis=-1),9.999999747378752e-6)[...,None],0,1)
        high=np.clip(((restored@LUMA_AP1)-.5)*.6666666865348816,0,1)[...,None]
        output=np.clip(output+(normalized-output)*high,0,1);trace["highlight_normalization"]=output
    if not np.isfinite(output).all():
        raise ValueError("Native candidate produced nonfinite output")
    return (output,trace) if return_trace else output


def build_candidate_lattice(dimension=32,parameters=None,curves=None):
    if type(dimension) is not int or not 2<=dimension<=64:
        raise ValueError("Candidate LUT dimension must be an integer in [2,64]")
    parameters=neutral_candidate_parameters() if parameters is None else parameters
    curves=neutral_candidate_curves() if curves is None else curves
    green,blue,red=np.indices((dimension,dimension,dimension))
    lattice=np.stack([red,green,blue],axis=-1)/(dimension-1)
    rgb=evaluate_native_chain(decode_logc3(lattice),parameters,curves)
    rgba=np.concatenate([rgb,np.ones((*rgb.shape[:-1],1))],axis=-1).reshape(dimension,dimension*dimension,4).astype("<f4")
    receipt={"schemaVersion":1,"generator":"azure.endfield-native-color-grading","generatorVersion":GENERATOR_VERSION,
        "evidenceStatus":"candidate","shaderVariant":"TONEMAPPING_ACES_MODIFIED","dimension":dimension,
        "inputDomain":"logc3-full-log","outputDomain":"linear","parameters":parameters,"curves":curves,
        "operationOrder":["LogC3 decode","white balance","linear to ACES to AP1 matrices","contrast and filter","split tone",
            "channel mixer","shadows/midtones/highlights","lift/gamma/gain","hue/saturation curves","master/RGB curves",
            "modified ACES curve","saturation and output matrix","highlight normalization"],
        "remainingUnknowns":["Active runtime grading volume and raw uniform values","CPU authoring-to-uniform conversions",
            "Native LUT dimension and image format","Native curve texels, width and sampler state",
            "Runtime exposure and pre-exposure compensation multipliers"],
        "candidateAssumptions":["Explicit shader-space neutral uniforms","128-wide neutral curves with clamp-to-edge linear sampling",
            "Float64 CPU evaluation rounded to float32 storage"],
        "outputStatistics":{"minimum":float(rgb.min()),"maximum":float(rgb.max()),"finite":bool(np.isfinite(rgb).all())}}
    return rgba,receipt


def decode_captured_strip(payload,metadata):
    """Decode an explicit mip-zero RGBA16F strip with replay provenance.

    The caller verifies the grading binding and LogC input domain separately.
    Captured texels remain exact when expanded from half to float storage.
    """
    n=metadata.get('height')
    if (metadata.get('complete') is not True or metadata.get('format')!='R16G16B16A16_FLOAT'
        or type(n) is not int or not 2<=n<=64 or metadata.get('width')!=n*n
        or metadata.get('depth')!=1 or any(type(metadata.get(key)) is not int for key in ('width','depth','mip','slice','sample','bytes'))
        or any(metadata.get(key)!=0 for key in ('mip','slice','sample'))):
        raise ValueError('A complete mip-zero captured RGBA16F LUT strip is required')
    if (len(payload)!=n*n*n*8 or metadata.get('bytes')!=len(payload)
        or hashlib.sha256(payload).hexdigest()!=metadata.get('sha256')):
        raise ValueError('Captured LUT payload length or digest mismatch')
    pixels=np.frombuffer(payload,dtype='<f2').astype('<f4').reshape(n,n*n,4)
    if not np.isfinite(pixels).all() or ((pixels<0)|(pixels>1)).any():
        raise ValueError('Captured LUT pixels exceed the public linear output domain')
    return pixels


def write_azlut(path,pixels,input_fingerprint,*,evidence=1):
    pixels=np.asarray(pixels,dtype="<f4")
    if pixels.ndim!=3 or pixels.shape[2]!=4 or not 2<=pixels.shape[0]<=64 or pixels.shape[1]!=pixels.shape[0]**2:
        raise ValueError("LUT pixels require a complete N by N*N RGBA strip")
    if not np.isfinite(pixels).all() or ((pixels<0)|(pixels>1)).any() or not input_fingerprint or type(evidence) is not int or evidence not in (1,2):
        raise ValueError("LUT pixels and provenance must satisfy the public resource contract")
    header={"schemaVersion":1,"dimension":pixels.shape[0],"width":pixels.shape[1],"height":pixels.shape[0],
        "inputDomain":"logc3-full-log","outputDomain":"linear","layout":"strip-rgb","precision":"float32-le","channels":4,
        "source":{"generator":"azure.endfield-native-color-grading","generatorVersion":GENERATOR_VERSION,
            "inputFingerprint":input_fingerprint,"evidence":evidence}}
    encoded=json.dumps(header,separators=(",",":"),sort_keys=True).encode()
    Path(path).write_bytes(b"AZLUT001"+struct.pack("<I",len(encoded))+encoded+pixels.tobytes())


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    source_group=parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument("--source-shader",type=Path)
    source_group.add_argument("--captured-strip",type=Path)
    parser.add_argument("--capture-receipt",type=Path)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--dimension",type=int,default=32)
    parser.add_argument("--parameters",type=Path,help="JSON with complete shader-space parameters and optional curves")
    args=parser.parse_args()
    if args.captured_strip:
        if not args.capture_receipt or args.parameters:parser.error('Captured strips require --capture-receipt and explicit source texels')
        exported=json.loads(args.capture_receipt.read_text(encoding='utf-8'))
        if exported.get('ok') is not True:raise ValueError('Successful replay export receipt required')
        metadata=exported['data'];pixels=decode_captured_strip(args.captured_strip.read_bytes(),metadata)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        write_azlut(args.output,pixels,metadata['sha256'],evidence=2)
        receipt={'schemaVersion':1,'evidenceStatus':'runtime-captured','sourceExport':metadata,
            'sourceReceiptSha256':hashlib.sha256(args.capture_receipt.read_bytes()).hexdigest(),
            'inputDomain':'logc3-full-log','outputDomain':'linear','output':str(args.output.resolve()),
            'outputSha256':hashlib.sha256(args.output.read_bytes()).hexdigest(),
            'scope':'Exact captured LUT texels; exposure, bloom, vignette and sharpening are separate consumers'}
        args.output.with_suffix('.receipt.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({'output':str(args.output),'evidenceStatus':receipt['evidenceStatus'],'sha256':receipt['outputSha256']}));return
    source=args.source_shader.read_bytes()
    for token in (b"TONEMAPPING_ACES_MODIFIED",b"2.785084962844848",b"0.887121975421905",b"1.705051541328430"):
        if token not in source:
            raise ValueError("Source does not contain the expected recovered native shader operator")
    authored=json.loads(args.parameters.read_text(encoding="utf-8")) if args.parameters else {}
    if not isinstance(authored,dict) or set(authored)-{"parameters","curves"}:
        raise ValueError("Candidate configuration supports parameters and curves only")
    pixels,receipt=build_candidate_lattice(args.dimension,authored.get("parameters"),authored.get("curves"))
    source_hash=hashlib.sha256(source).hexdigest()
    fingerprint=hashlib.sha256(json.dumps({"shader":source_hash,"parameters":receipt["parameters"],"curves":receipt["curves"]},sort_keys=True).encode()).hexdigest()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    write_azlut(args.output,pixels,fingerprint)
    receipt.update(sourceShader=str(args.source_shader.resolve()),sourceShaderSha256=source_hash,inputFingerprint=fingerprint,
        output=str(args.output.resolve()),outputSha256=hashlib.sha256(args.output.read_bytes()).hexdigest())
    args.output.with_suffix(".receipt.json").write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"output":str(args.output),"evidenceStatus":"candidate","sha256":receipt["outputSha256"]}))


if __name__=="__main__":
    main()
