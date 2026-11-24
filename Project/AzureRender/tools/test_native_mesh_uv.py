"""Restore authored UVs only across explicit matching primitive topology."""
import copy
import struct
import unittest
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from endfield_native.mesh_uv import restore_primary_uv
from convert_material_semantics import write_asset,read_asset


def temporary_folder():
    root=Path(__file__).resolve().parents[1]/'build/runs'
    root.mkdir(parents=True,exist_ok=True)
    return tempfile.TemporaryDirectory(prefix='mesh-uv-test-',dir=root)


def fixture(strided=False):
    binary=bytearray();views=[];accessors=[]
    def add(values,kind,width,component=5126,stride=None):
        binary.extend(b'\0'*(-len(binary)%4));offset=len(binary)
        for value in values:binary.extend(struct.pack('<'+('f' if component==5126 else 'H')*len(value),*value))
        view=dict(buffer=0,byteOffset=offset,byteLength=len(binary)-offset)
        if stride:view['byteStride']=stride
        views.append(view);accessors.append(dict(bufferView=len(views)-1,componentType=component,count=len(values),type=kind))
        return len(accessors)-1
    position=add([(0,0,0),(1,0,0),(0,1,0)],'VEC3',3)
    uv=add([(0,.1,99),(1,.3,99),(.2,.8,99)] if strided else [(0,.1),(1,.3),(.2,.8)],'VEC2',2,stride=12 if strided else None)
    tangent=add([(1,0,0,1)]*3,'VEC4',4)
    indices=add([(0,),(1,),(2,)],'SCALAR',1,5123)
    binary.extend(b'\0'*(-len(binary)%4))
    primitive=dict(attributes=dict(POSITION=position,TEXCOORD_0=uv,TANGENT=tangent),indices=indices,material=0)
    document=dict(asset=dict(version='2.0'),materials=[dict(name='surface'),dict(name='other')],
        meshes=[dict(primitives=[primitive,dict(primitive,material=1)])],
        accessors=accessors,bufferViews=views,buffers=[dict(byteLength=len(binary))],
        nodes=[dict(mesh=0,children=[1]),dict(name='joint')],skins=[dict(joints=[1])],
        animations=[dict(name='retained-input',samplers=[],channels=[])])
    return document,bytes(binary)


def patch_uv(document,binary,values):
    result=bytearray(binary);accessor=document['accessors'][1];view=document['bufferViews'][accessor['bufferView']]
    for i,value in enumerate(values):struct.pack_into('<2f',result,view['byteOffset']+i*view.get('byteStride',8),*value)
    return bytes(result)


def read_uv(document,binary,primitive=0):
    index=document['meshes'][0]['primitives'][primitive]['attributes']['TEXCOORD_0'];a=document['accessors'][index];v=document['bufferViews'][a['bufferView']]
    return [struct.unpack_from('<2f',binary,v['byteOffset']+i*v.get('byteStride',8)) for i in range(a['count'])]


class MeshUvTests(unittest.TestCase):
    def restore(self,doc,binary,reference,reference_binary,selected=('name:surface',)):
        return restore_primary_uv(doc,binary,reference,reference_binary,selected)

    def test_reference_uv_and_unselected_shared_accessor(self):
        doc,binary=fixture();reference=copy.deepcopy(doc);expected=[(.125,.25),(.5,.625),(.75,.875)];ref_binary=patch_uv(reference,binary,expected);before=copy.deepcopy(doc)
        result,payload,report=self.restore(doc,binary,reference,ref_binary)
        self.assertEqual(read_uv(result,payload),expected)
        self.assertEqual(read_uv(result,payload,1),read_uv(doc,binary,1))
        self.assertEqual(payload[:len(binary)],binary)
        self.assertEqual(doc,before)
        self.assertEqual(result['meshes'][0]['primitives'][1],doc['meshes'][0]['primitives'][1])
        for key in ['nodes','skins','animations','materials']:self.assertEqual(result[key],doc[key])
        for key in ['POSITION','TANGENT']:self.assertEqual(result['meshes'][0]['primitives'][0]['attributes'][key],doc['meshes'][0]['primitives'][0]['attributes'][key])
        self.assertEqual(report['vertices'],3)

    def test_strided_source_and_target(self):
        doc,binary=fixture(True);reference,ref_binary=fixture(True);ref_binary=patch_uv(reference,ref_binary,[(.125,.25),(.5,.625),(.75,.875)])
        result,payload,_=self.restore(doc,binary,reference,ref_binary)
        self.assertEqual(read_uv(result,payload),[(.125,.25),(.5,.625),(.75,.875)])

    def test_topology_and_tangent_sign_mismatch_rejected(self):
        doc,binary=fixture()
        for kind in ['index','sign','count','primitive-count']:
            with self.subTest(kind=kind):
                reference=copy.deepcopy(doc);payload=bytearray(binary)
                if kind=='index':struct.pack_into('<H',payload,reference['bufferViews'][3]['byteOffset'],2)
                elif kind=='sign':struct.pack_into('<f',payload,reference['bufferViews'][2]['byteOffset']+12,-1)
                elif kind=='count':reference['accessors'][0]['count']=2
                else:reference['meshes'][0]['primitives'].append(copy.deepcopy(reference['meshes'][0]['primitives'][0]))
                with self.assertRaises(ValueError):self.restore(doc,binary,reference,bytes(payload))

    def test_invalid_uv_accessors_rejected_without_mutation(self):
        doc,binary=fixture()
        for kind in ['negative-offset','truncated-view','short-stride','unaligned','external-buffer','sparse','integer','normalized','nonfinite','mismatched-count']:
            with self.subTest(kind=kind):
                bad=copy.deepcopy(doc);payload=bytearray(binary);a=bad['accessors'][1];v=bad['bufferViews'][1]
                if kind=='negative-offset':a['byteOffset']=-4
                elif kind=='truncated-view':v['byteLength']=4
                elif kind=='short-stride':v['byteStride']=4
                elif kind=='unaligned':a['byteOffset']=1
                elif kind=='external-buffer':v['buffer']=1
                elif kind=='sparse':a['sparse']={}
                elif kind=='integer':a['componentType']=5123
                elif kind=='normalized':a['normalized']=True
                elif kind=='nonfinite':struct.pack_into('<f',payload,v['byteOffset'],float('nan'))
                else:a['count']=2
                before=copy.deepcopy(bad)
                with self.assertRaises(ValueError):self.restore(bad,bytes(payload),doc,binary)
                self.assertEqual(bad,before)

    def test_invalid_or_ambiguous_slots_rejected(self):
        doc,binary=fixture()
        for selected in [(),('name:missing',),('name:surface','name:surface'),(False,)]:
            with self.subTest(selected=selected),self.assertRaises(ValueError):self.restore(doc,binary,doc,binary,selected)

    def test_source_failure_is_checked(self):
        doc,binary=fixture();reference=copy.deepcopy(doc);reference['bufferViews'][1]['byteLength']=0
        with self.assertRaises(ValueError):self.restore(doc,binary,reference,binary)

    def test_multiple_material_slots_use_independent_new_references(self):
        doc,binary=fixture();reference,ref_binary=fixture();ref_binary=patch_uv(reference,ref_binary,[(.125,.25),(.5,.625),(.75,.875)])
        result,payload,report=self.restore(doc,binary,reference,ref_binary,('name:surface','name:other'))
        self.assertEqual(read_uv(result,payload),read_uv(result,payload,1))
        self.assertNotEqual(result['meshes'][0]['primitives'][0]['attributes']['TEXCOORD_0'],result['meshes'][0]['primitives'][1]['attributes']['TEXCOORD_0'])
        self.assertEqual(report['vertices'],6)

    def test_unlit_geometry_without_tangents_and_mismatched_presence(self):
        doc,binary=fixture();reference=copy.deepcopy(doc)
        for primitive in doc['meshes'][0]['primitives']:primitive['attributes'].pop('TANGENT',None)
        with self.assertRaises(ValueError):self.restore(doc,binary,reference,binary)
        for primitive in reference['meshes'][0]['primitives']:primitive['attributes'].pop('TANGENT',None)
        expected=[(.125,.25),(.5,.625),(.75,.875)];result,payload,report=self.restore(doc,binary,reference,patch_uv(reference,binary,expected))
        self.assertEqual(read_uv(result,payload),expected)
        self.assertFalse(report['primitives'][0]['tangentSignVerified'])

    def test_cli_generates_new_glb_and_protects_existing_outputs(self):
        doc,binary=fixture();expected=[(.125,.25),(.5,.625),(.75,.875)];ref_binary=patch_uv(doc,binary,expected)
        with temporary_folder() as folder:
            root=Path(folder);source=root/'input.glb';reference=root/'reference.glb';output=root/'output.glb';report=root/'report.json'
            tail=lambda b:struct.pack('<II',len(b),0x004e4942)+b
            write_asset(source,doc,tail(binary));write_asset(reference,doc,tail(ref_binary));original=source.read_bytes()
            command=[sys.executable,str(Path(__file__).parent/'endfield_native/mesh_uv.py'),'--input',str(source),'--reference',str(reference),'--slot','name:surface','--output',str(output),'--report',str(report)]
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            restored,restored_tail=read_asset(output);self.assertEqual(read_uv(restored,restored_tail[8:]),expected)
            self.assertEqual(source.read_bytes(),original);self.assertEqual(json.loads(report.read_text())['vertices'],3)
            before=output.read_bytes();result=subprocess.run(command,capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0);self.assertEqual(output.read_bytes(),before)

    def test_cli_rejects_invalid_source_without_creating_outputs(self):
        doc,binary=fixture();bad=copy.deepcopy(doc);bad['bufferViews'][1]['byteLength']=0
        with temporary_folder() as folder:
            root=Path(folder);source=root/'input.glb';reference=root/'reference.glb';output=root/'output.glb';report=root/'report.json';tail=struct.pack('<II',len(binary),0x004e4942)+binary
            write_asset(source,doc,tail);write_asset(reference,bad,tail)
            result=subprocess.run([sys.executable,str(Path(__file__).parent/'endfield_native/mesh_uv.py'),'--input',str(source),'--reference',str(reference),'--slot','name:surface','--output',str(output),'--report',str(report)],capture_output=True)
            self.assertNotEqual(result.returncode,0);self.assertFalse(output.exists());self.assertFalse(report.exists())

    def test_format_and_enum_types_are_explicit(self):
        doc,binary=fixture()
        for kind in ['version','float-component','boolean-mode','invalid-mode']:
            with self.subTest(kind=kind):
                bad=copy.deepcopy(doc)
                if kind=='version':bad['asset']['version']='1.0'
                elif kind=='float-component':bad['accessors'][1]['componentType']=5126.0
                elif kind=='boolean-mode':bad['meshes'][0]['primitives'][0]['mode']=True
                else:bad['meshes'][0]['primitives'][0]['mode']=7
                with self.assertRaises(ValueError):self.restore(bad,binary,bad,binary)


if __name__=='__main__':unittest.main()
