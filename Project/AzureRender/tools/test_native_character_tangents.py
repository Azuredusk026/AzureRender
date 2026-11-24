"""Public fixtures for exact source vertex correspondence and tangent retention."""
import copy
import struct
import unittest
import numpy as np
from endfield_native import character_tangents as subject


class TangentTests(unittest.TestCase):
    def fixture(self):
        payload = struct.pack('<9f', 0,0,0, 1,0,0, 0,1,0)+struct.pack('<3H',0,1,2)+b'\0\0'
        doc = {'buffers': [{'byteLength': len(payload)}],
               'bufferViews': [{'buffer':0, 'byteOffset':0, 'byteLength':36}, {'buffer':0, 'byteOffset':36, 'byteLength':6}],
               'accessors': [{'bufferView':0, 'componentType':5126, 'type':'VEC3', 'count':3},
                             {'bufferView':1, 'componentType':5123, 'type':'SCALAR', 'count':3}],
               'meshes':[{'name':'fixture', 'primitives':[{'attributes':{'POSITION':0}, 'indices':1}]}],
               'nodes':[{'mesh':0}], 'skins':[{'joints':[1]}], 'animations':[{'name':'idle'}],
               'materials':[{'name':'retained'}]}
        tail = struct.pack('<II',len(payload),0x004e4942)+payload
        return doc, tail

    def test_append_retains_original_streams_and_animation(self):
        doc, tail = self.fixture();before = copy.deepcopy(doc)
        values = np.array([[1,0,0,1], [0,1,0,-1], [1,0,0,-1]],dtype='<f4')
        result, newtail = subject.append_tangents(doc, tail, {(0,0):values})
        self.assertEqual(doc,before)
        self.assertEqual(newtail[8:8+len(tail)-8],tail[8:])
        for key in ('nodes','skins','animations','materials'):
            self.assertEqual(result[key],doc[key])
        index=result['meshes'][0]['primitives'][0]['attributes']['TANGENT']
        np.testing.assert_array_equal(subject.accessor(result,newtail,index),values)
        self.assertEqual(result['accessors'][:2],doc['accessors'])

    def test_correspondence_must_match_every_original_attribute_and_index(self):
        doc,tail=self.fixture();other=copy.deepcopy(doc)
        subject.require_same_geometry(doc,tail,other,tail)
        modified=bytearray(tail);struct.pack_into('<f',modified,8,4)
        with self.assertRaises(ValueError):subject.require_same_geometry(doc,tail,other,bytes(modified))
        modified=bytearray(tail);struct.pack_into('<H',modified,8+36,2)
        with self.assertRaises(ValueError):subject.require_same_geometry(doc,tail,other,bytes(modified))

    def test_tangent_transform_keeps_authored_sign_and_rejects_invalid_source(self):
        source=np.array([[1,0,0,-1]],dtype='<f4')
        rotation=np.array([[0,-1,0],[1,0,0],[0,0,1]],dtype=np.float64)
        np.testing.assert_allclose(subject.transform_tangents(source,rotation),[[0,1,0,-1]],atol=1e-7)
        np.testing.assert_allclose(subject.transform_tangents(source,np.diag([-1,1,1])),[[-1,0,0,1]],atol=1e-7)
        with self.assertRaises(ValueError):subject.transform_tangents(np.array([[0,0,0,1]]),rotation)
        with self.assertRaises(ValueError):subject.transform_tangents(np.array([[1,0,0,0]]),rotation)

    def test_exact_float_exported_vertex_ids_are_validated(self):
        np.testing.assert_array_equal(subject.source_vertex_indices(np.array([2.,0.,1.]),3),[2,0,1])
        with self.assertRaises(ValueError):subject.source_vertex_indices(np.array([.5]),3)
        with self.assertRaises(ValueError):subject.source_vertex_indices(np.array([3.]),3)
        with self.assertRaises(ValueError):subject.source_vertex_indices(np.array([float('nan')]),3)


if __name__=='__main__':unittest.main()
