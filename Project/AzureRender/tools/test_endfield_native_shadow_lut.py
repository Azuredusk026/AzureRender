import copy
from io import BytesIO
import unittest
from PIL import Image
from endfield_native.shadow_lut import restore_shadow_luts


class ShadowLutTests(unittest.TestCase):
    def inputs(self):
        material={'source':'cab-m','path_id':1,'name':'face'}
        image={'source':'cab-t','path_id':2,'name':'lookup'}
        document={'materials':[{'name':'face','extras':{'azureRenderMaterial':{'schemaVersion':2,'class':'face','faceSdf':{'schemaVersion':2}}}}],
                  'nodes':[{'name':'head'}],'animations':[{'name':'idle'}]}
        audit={'materials':[{'object':material,'textures':[{'slot':'_ShadowLutTex','reference':{'resolved':image},'color_space':1,'scale':[1,1],'offset':[0,0]}],
                            'floats':[{'name':'_UseShadowLutTex','value':1}]}]}
        sources=[{'object':material,'data':{'m_ValidKeywords':['_SHADOW_LUT_TEX']}}]
        stream=BytesIO();Image.new('RGBA',(4,2),(128,64,32,255)).save(stream,format='PNG')
        artifacts=[{'object':image,'vertical_flip':True}]
        return document,audit,sources,{('cab-t',2):stream.getvalue()},artifacts

    def test_enabled_native_slot_reaches_independent_profile_without_structural_changes(self):
        args=self.inputs();original=copy.deepcopy(args[0])
        result,report=restore_shadow_luts(*args)
        self.assertEqual(args[0],original)
        self.assertEqual(result['nodes'],original['nodes']);self.assertEqual(result['animations'],original['animations'])
        profile=result['materials'][0]['extras']['azureRenderMaterial']
        self.assertEqual(profile['faceSdf'],original['materials'][0]['extras']['azureRenderMaterial']['faceSdf'])
        self.assertEqual(profile['shadowColorLut'],{'schemaVersion':1,'texture':0,'flipV':True})
        self.assertEqual(report['materials'][0]['source_keyword'],'_SHADOW_LUT_TEX')
        self.assertFalse(report['native_npr_composition_verified'])

    def test_disabled_keyword_keeps_material_unchanged(self):
        args=list(self.inputs());args[2][0]['data']['m_ValidKeywords']=[]
        result,report=restore_shadow_luts(*args)
        self.assertEqual(result,args[0]);self.assertEqual(report['materials'],[])

    def test_invalid_source_lut_is_rejected(self):
        for field,value in [('color_space',0),('scale',[2,1]),('offset',[.5,0])]:
            args=list(self.inputs());args[1]['materials'][0]['textures'][0][field]=value
            with self.assertRaises(ValueError):restore_shadow_luts(*args)
        args=list(self.inputs());args[4][0].pop('vertical_flip')
        with self.assertRaises(ValueError):restore_shadow_luts(*args)
        args=list(self.inputs());args[3]={}
        with self.assertRaises(ValueError):restore_shadow_luts(*args)
        args=list(self.inputs());args[1]['materials'][0]['floats'][0]['value']=0
        with self.assertRaises(ValueError):restore_shadow_luts(*args)


if __name__=='__main__':unittest.main()
