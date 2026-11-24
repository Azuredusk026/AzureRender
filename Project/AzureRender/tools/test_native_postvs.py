import copy,hashlib,json,struct,tempfile,unittest
from pathlib import Path
import numpy as np
from endfield_native.postvs import read_postvs

class PostVsTests(unittest.TestCase):
    def fixture(self,root):
        vertex=b'padding!'+struct.pack('<12f',1,2,3,1,.1,.2,4,5,6,1,.3,.4)
        index=b'xx'+struct.pack('<3H',0,1,0)
        for name,data in [('v.bin',vertex),('i.bin',index)]: (root/name).write_bytes(data)
        manifest={'schema_version':1,'complete':True,'stage':'VSOut','output_signature':[
            {'semanticIdxName':'SV_Position','compCount':4,'varType':0,'stream':0,'perPrimitiveRate':False},
            {'semanticIdxName':'TEXCOORD0','compCount':2,'varType':0,'stream':0,'perPrimitiveRate':False}],
            'buffers':[{'rid':r,'file':f,'bytes':len(d),'sha256':hashlib.sha256(d).hexdigest()} for r,f,d in [('v','v.bin',vertex),('i','i.bin',index)]],
            'instances':[{'instance':0,'files':{'vertex':'v.bin','index':'i.bin'},'mesh':{
                'vertexResourceId':'v','indexResourceId':'i','vertexByteOffset':8,'vertexByteStride':24,
                'vertexByteSize':48,'indexByteOffset':2,'indexByteStride':2,'indexByteSize':6,
                'baseVertex':0,'numIndices':3,'topology':5,'format':{'compByteWidth':4,'compType':1,'type':0}}}]}
        return manifest
    def read(self,root,data):
        path=root/'manifest.json';path.write_text(json.dumps(data));return read_postvs(path)
    def test_offsets_stride_and_duplicate_indices(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);result=self.read(root,self.fixture(root))
            np.testing.assert_array_equal(result['indices'],[0,1,0])
            np.testing.assert_allclose(result['attributes']['TEXCOORD0'],[[.1,.2],[.3,.4]])
            self.assertEqual(result['attributes']['SV_Position'].shape,(2,4))
    def test_base_vertex_and_shared_register_packing(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);data=self.fixture(root);data['instances'][0]['mesh']['baseVertex']=1
            data['instances'][0]['mesh']['numIndices']=1
            result=self.read(root,data);np.testing.assert_array_equal(result['indices'],[1])
            self.assertEqual(result['attributes']['TEXCOORD0'].shape,(2,2))
    def test_rejects_corruption_ranges_and_unsupported_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);original=self.fixture(root)
            for mutate in [lambda d:d.update(complete=False),lambda d:d['buffers'][0].update(sha256='bad'),
                lambda d:d['buffers'][0].update(file='../v.bin'),lambda d:d['instances'][0]['mesh'].update(numIndices=4),
                lambda d:d['instances'][0]['mesh'].update(baseVertex=2),lambda d:d['instances'][0]['mesh'].update(vertexByteStride=20),
                lambda d:d['output_signature'][0].update(varType=2),lambda d:d['output_signature'][0].update(stream=1)]:
                data=copy.deepcopy(original);mutate(data)
                with self.assertRaises(ValueError):self.read(root,data)
    def test_uint_semantics_preserve_bits(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);data=self.fixture(root);data['output_signature'][1]['varType']=4
            result=self.read(root,data)
            self.assertEqual(result['attributes']['TEXCOORD0'].dtype,np.dtype('<u4'))
            np.testing.assert_array_equal(result['attributes']['TEXCOORD0'][0],np.array([.1,.2],dtype='<f4').view('<u4'))
    def test_replay_position_first_with_late_shader_position(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);data=self.fixture(root)
            data['output_signature'].reverse()
            result=self.read(root,data)
            np.testing.assert_array_equal(result['attributes']['SV_Position'],[[1,2,3,1],[4,5,6,1]])
            np.testing.assert_allclose(result['attributes']['TEXCOORD0'],[[.1,.2],[.3,.4]])

if __name__=='__main__':unittest.main()
