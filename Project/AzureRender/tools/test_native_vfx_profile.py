"""Validate pinned VFX source selection independently of Blender."""
import copy
import unittest

from endfield_native.vfx_profile import unlit_vfx_profile, layered_vfx_profile, active_vfx_layers


def layered_source():
    record=source();f=record['Floats']
    for key in ('_UseInkWash','_UseDissolve','_UseDisturb','_UseFog','_UseNearCameraFade','_UseFresnel',
                '_UseAnimBreathing','_UseBright','_UseEdgeColor','_UsePolarUV','_UseScreenUV',
                '_UsePosYAsScreenV','_UseVertexOffset','_UseVertexOffsetMask','_UseTrail','_UseVATColorTex',
                '_HoudiniVATInParticle','_UseVFXPortal','_UseWaterBlend','_UseParticleDisturb','_UseWeightTex'):
        f[key]=0
    f.update(_UseSoftBlend=1,_SoftDistance=1.5,_SoftBias=0,_TintColorIntensity=1.3,_TintColorAlpha=1,
             _ExpThreshold=1,_ExpIntensity=3,_IgnorePostExposure=1,_IsSceneEffect=1,
             _UseMask=0,_UseBlend=0,_SrcBlend=1,_DstBlend=10,_AlphaSrcBlend=1,_AlphaDstBlend=10,
             _BlendMode=0,_ZWrite=0,_CullMode=0,_UseMainTexAsAlpha=0)
    for i in range(6):f[f'_UseSampleTex{i}']=0
    record['RenderState']={'ValidKeywords':['_USE_SOFTBLEND'],'CustomRenderQueue':3000}
    record['Colors']={'_MainTexUVWeights':[1,0,0,0],'_MainTexUVRotateMat':[1,0,0,1],
        '_MainTexUVSpeed':[0,0,0,0],'_TintColor':[1,1,1,1],'_BlendTint':[2,1,.5,.5]}
    record['Textures']=[dict(Property='_MainTex',Texture=dict(Source='texture-source',PathId=7,Type='Texture2D'),Scale=[1,1],Offset=[0,0])]
    return record


def sample_source(role=3):
    r=layered_source();f=r['Floats'];f['_UseSampleTex0']=1;f['_SampleTex0Use']=role
    f[f'_SampleTex0UseWeight{role}']=1;f['_UseSampleTex0AsAlpha']=1;f['_SampleTex0MipmapBias']=0
    f['_UseMask']=int(role==3);f['_UseBlend']=int(role==4)
    r['RenderState']['ValidKeywords'].append('_SAMPLE_TEX0')
    r['Colors'].update(_SampleTex0UVWeights=[1,0,0,0],_SampleTex0UVRotateMat=[0,1,-1,0],_SampleTex0UVSpeed=[0,.1,0,0])
    r['Textures'] += [dict(Property='_SampleTex0',Texture=dict(Source='texture-source',PathId=8,Type='Texture2D'),Scale=[99,99],Offset=[9,9]),
        dict(Property='_MaskTex' if role==3 else '_BlendTex',Texture=dict(Source='unused',PathId=0,Type='Texture'),Scale=[2,3],Offset=[.2,.3])]
    return r


class NativeVfxLayersTests(unittest.TestCase):
    def convert(self,r,indices=None,clock=None,color_encoding='linear'):
        return layered_vfx_profile(r,{'_MainTex':0,**({'_SampleTex0':1} if r['Floats']['_UseSampleTex0'] else {})} if indices is None else indices,
            clock={'rate':1,'offsetSeconds':0} if clock is None else clock,color_encoding=color_encoding)

    def test_authored_colors_match_captured_gamma_and_hdr_inputs(self):
        r=sample_source(4);r['Colors']['_TintColor']=[.7,.7,.7,1];r['Colors']['_BlendTint']=[23.968628,0,0,1]
        p=self.convert(r,color_encoding='authored-srgb-hdr')['surface']
        self.assertAlmostEqual(p['tint'][0],.4479884207248688*1.3,places=6)
        self.assertEqual(p['blendTint'],[1084.4637451171875,0,0,1])
        with self.assertRaises(ValueError):self.convert(r,color_encoding='guess')

    def test_main_layer_is_typed_and_source_owned(self):
        r=layered_source();frozen=copy.deepcopy(r);p=self.convert(r)
        self.assertEqual(p['features'],['unlit-surface','unlit-texture-layers'])
        self.assertEqual(p['surface']['tint'],[1.3,1.3,1.3,1]);self.assertEqual(r,frozen)
        self.assertEqual(p['surface']['layers'][0]['uv']['scaleOffset'],[1,1,0,0])

    def test_sample_uses_role_st_and_own_texture_and_transform(self):
        for role,name in [(3,'mask'),(4,'blend')]:
            p=self.convert(sample_source(role))['surface'];row=p['layers'][1]
            self.assertEqual(row['role'],name);self.assertEqual(row['texture'],1)
            self.assertEqual(row['uv']['scaleOffset'],[2,3,.2,.3]);self.assertEqual(row['uv']['rotation'],[0,1,-1,0])
            self.assertEqual(row['uv']['speed'],[0,.1]);self.assertEqual(row['sampleMode'],'red-as-alpha')
            self.assertEqual(p['blendTint'],[2,1,.5,.5] if role==4 else [0,0,0,0])

    def test_inactive_serialized_layers_are_ignored(self):
        r=layered_source();r['Colors']['_SampleTex1UVSpeed']=[99,2,3,4];r['Floats']['_UseDisturb2']=1
        self.assertEqual(len(active_vfx_layers(r)),1)

    def test_unsupported_active_features_are_rejected(self):
        original=layered_source()
        for key in [k for k,v in original['Floats'].items() if k.startswith('_Use') and v==0 and k not in ['_UseMainTexAsAlpha','_UseMask','_UseBlend'] and not k.startswith('_UseSampleTex')]:
            r=copy.deepcopy(original);r['Floats'][key]=1
            with self.subTest(key=key),self.assertRaises(ValueError):self.convert(r)

    def test_bad_clock_and_texture_ownership(self):
        for clock in [{'rate':True,'offsetSeconds':0},{'rate':float('nan'),'offsetSeconds':0},{'rate':1}, {'rate':1,'offsetSeconds':0,'extra':0}]:
            with self.subTest(clock=clock),self.assertRaises(ValueError):self.convert(layered_source(),clock=clock)
        for indices in [{},{'_MainTex':True},{'_MainTex':-1},{'_MainTex':0,'unused':1}]:
            with self.subTest(indices=indices),self.assertRaises(ValueError):self.convert(layered_source(),indices=indices)

    def test_invalid_layers_and_role_weights(self):
        cases=[]
        r=sample_source();r['Colors']['_SampleTex0UVWeights']=[0,1,0,0];cases.append(r)
        r=sample_source();r['Floats']['_SampleTex0UseWeight4']=1;cases.append(r)
        r=sample_source();r['Floats']['_SampleTex0Use']=0;cases.append(r)
        r=sample_source();r['Floats']['_UseSampleTex1']=1;cases.append(r)
        r=layered_source();r['Floats']['_UseSoftBlend']=0;cases.append(r)
        r=layered_source();r['RenderState']['ValidKeywords'].append('UNKNOWN');cases.append(r)
        r=layered_source();r['Floats']['_SrcBlend']=5;cases.append(r)
        r=layered_source();r['Textures'][0]['Texture']['PathId']=0;cases.append(r)
        r=layered_source();r['Colors']['_MainTexUVSpeed']=[0,0,1,0];cases.append(r)
        for r in cases:
            with self.subTest(record=r),self.assertRaises(ValueError):self.convert(r)


def source():
    return {
        'Name': 'IndependentVfxFixture',
        'Shader': {'Name': 'HGRP/Effect/VFXBaseV2',
                   'Source': 'CAB-9fe1ba4f3ce8edb2f54eb3e35709a336',
                   'PathId': -1430105248647086886},
        'Floats': {'_UseLighting': 0, '_UseForceDynamicLighting': 0,
                   '_EnvLightStrength': 0},
    }


class NativeVfxProfileTests(unittest.TestCase):
    def test_pinned_source_produces_explicit_unlit_surface(self):
        record = source()
        frozen = copy.deepcopy(record)
        self.assertEqual(unlit_vfx_profile(record), {
            'schemaVersion': 2, 'class': 'emissive',
            'features': ['emissive-mask', 'unlit-surface'],
            'surface': {'schemaVersion': 1, 'lighting': 'unlit'},
        })
        self.assertEqual(record, frozen)

    def test_source_selection_uses_identity(self):
        for key, value in [('Name', 'HGRP/Lit'), ('Source', 'other'),
                           ('PathId', 1), ('PathId', str(-1430105248647086886))]:
            with self.subTest(key=key, value=value):
                record = source()
                record['Shader'][key] = value
                with self.assertRaisesRegex(ValueError, 'identity'):
                    unlit_vfx_profile(record)

    def test_lighting_switches_are_required_and_zero(self):
        for key in source()['Floats']:
            for value in [None, True, '0', float('nan'), float('inf'), -1, .1, 1]:
                with self.subTest(key=key, value=value):
                    record = source()
                    if value is None:
                        record['Floats'].pop(key)
                    else:
                        record['Floats'][key] = value
                    with self.assertRaisesRegex(ValueError, key):
                        unlit_vfx_profile(record)

    def test_fresh_result_has_independent_ownership(self):
        first = unlit_vfx_profile(source())
        first['features'].clear()
        first['surface']['lighting'] = 'other'
        self.assertEqual(unlit_vfx_profile(source())['surface']['lighting'], 'unlit')
        self.assertEqual(len(unlit_vfx_profile(source())['features']), 2)


if __name__ == '__main__':
    unittest.main()
