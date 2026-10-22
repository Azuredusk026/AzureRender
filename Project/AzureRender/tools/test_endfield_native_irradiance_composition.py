import copy
import math
import struct
import unittest

try:
    from endfield_native import irradiance_composition as composition
except ImportError:
    composition = None


def parameters(**values):
    result = dict(version=1, worldPosition=[0., 0., 0.],
                  cameraPosition=[0., 0., 0.], chunkOrigin=[0., 0., 0.],
                  centerScales=[1., 4., 16.], enabled=True,
                  sampledLayers=[dict(amplitude=[1., 1., 1.],
                      directionalRgba=[[0., 0., 0., .5] for _ in range(3)]) for _ in range(3)],
                  defaultCoefficients=[[2., 4., 6., 8.],
                                       [1., 2., 3., 4.],
                                       [3., 6., 9., 12.]])
    result.update(values)
    return copy.deepcopy(result)


class IrradianceCompositionTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(composition, 'NR4-G5 public composition module is not implemented')
        self.assertTrue(callable(getattr(composition, 'compose_native_irradiance', None)),
                        'NR4-G5 public composition entry is not implemented')
        self.assertTrue(callable(getattr(composition, 'decode_native_irradiance_sample', None)),
                        'NR4-G5 sample entry is not implemented')

    def compose(self, value):
        return composition.compose_native_irradiance(value)

    def assertRows(self, got, want):
        self.assertEqual(len(got), len(want))
        for a, b in zip(got, want):
            self.assertEqual(len(a), len(b))
            for x, y in zip(a, b):
                self.assertAlmostEqual(x, y, places=12)

    def test_disabled_uses_explicit_default_and_retains_all_alpha(self):
        value = parameters(enabled=False)
        value['sampledLayers'][1]['directionalRgba'][2][3] = .75
        result = self.compose(value)
        self.assertRows(result['coefficients'], [[2, 4, 6, 8], [1, 2, 3, 4], [3, 6, 9, 12]])
        self.assertEqual(result['layerWeights'], [0, 0, 0])
        self.assertEqual(result['directionalAlpha'], [[.5]*3, [.5, .5, .75], [.5]*3])
        self.assertEqual(result['defaultMix'], dict(u=0., v=1.))
        self.assertFalse(result['volumeActive'])

    def test_interior_half_alpha_uses_finest_field(self):
        result = self.compose(parameters())
        self.assertRows(result['coefficients'], [[-2, -2, -2, 1]]*3)
        self.assertEqual(result['layerWeights'], [1, 0, 0])
        self.assertEqual(result['clipmapFades'], [0, 0, 0])
        self.assertEqual(result['defaultMix'], dict(u=0., v=0.))
        self.assertTrue(result['volumeActive'])

    def test_stationary_room_alpha_uses_cross_coupled_default(self):
        value = parameters()
        for layer in value['sampledLayers']:
            for d in layer['directionalRgba']: d[3] = 1.
        result = self.compose(value)
        self.assertRows(result['coefficients'], [[-1,4,1,6.5],[-1.5,1,-.5,3.75],[-.5,7,2.5,9.25]])
        self.assertEqual(result['defaultMix'], dict(u=1., v=.5))

    def test_default_control_only_uses_first_direction_group_alpha(self):
        value = parameters()
        before = self.compose(value)
        for layer in value['sampledLayers']:
            layer['directionalRgba'][1][3] = 0.
            layer['directionalRgba'][2][3] = 1.
        after = self.compose(value)
        self.assertEqual(before['coefficients'], after['coefficients'])
        self.assertEqual(after['directionalAlpha'], [[.5,0,1]]*3)

    def test_fine_transition_combines_explicit_two_fields(self):
        value = parameters(worldPosition=[30.,0.,0.], defaultCoefficients=[[0.]*4]*3)
        value['sampledLayers'][1]['amplitude'] = [3.]*3
        result = self.compose(value)
        self.assertEqual(result['layerWeights'], [.5,.5,0])
        self.assertRows(result['coefficients'], [[-4,-4,-4,2]]*3)

    def test_middle_transition_combines_explicit_two_fields(self):
        value = parameters(worldPosition=[120.,0.,0.], defaultCoefficients=[[0.]*4]*3)
        value['sampledLayers'][1]['amplitude'] = [3.]*3
        value['sampledLayers'][2]['amplitude'] = [5.]*3
        result = self.compose(value)
        self.assertEqual(result['layerWeights'], [0,.5,.5])
        self.assertRows(result['coefficients'], [[-8,-8,-8,4]]*3)

    def test_outer_transition_blends_native_default_control(self):
        value = parameters(worldPosition=[480.,0.,0.])
        result = self.compose(value)
        self.assertEqual(result['layerWeights'], [0,0,.5])
        self.assertEqual(result['defaultMix'], dict(u=0.,v=.5))
        self.assertRows(result['coefficients'], [[0,1,2,4.5],[-.5,0,.5,2.5],[.5,2,3.5,6.5]])

    def test_moving_camera_uses_explicit_center_transform(self):
        a = self.compose(parameters(worldPosition=[29.,0.,0.]))
        b = self.compose(parameters(worldPosition=[29.,0.,0.], cameraPosition=[1.,0.,0.]))
        c = self.compose(parameters(worldPosition=[29.,0.,0.], cameraPosition=[1.,0.,0.], chunkOrigin=[1.,0.,0.]))
        self.assertEqual(a['layerWeights'], [1,0,0])
        self.assertEqual(b['layerWeights'], [.5,.5,0])
        self.assertEqual(c['layerWeights'], [1,0,0])

    def test_all_horizontal_vertical_and_signed_boundaries(self):
        for axis in range(3):
            boundaries = (29.,31.,116.,124.,464.,496.) if axis != 1 else (13.,15.,52.,60.,208.,240.)
            expected = ([1,0,0],[0,1,0],[0,1,0],[0,0,1],[0,0,1],[0,0,0])
            for sign in (-1,1):
                for boundary, want in zip(boundaries,expected):
                    value=parameters();value['worldPosition'][axis]=sign*boundary
                    with self.subTest(axis=axis,sign=sign,boundary=boundary):
                        self.assertEqual(self.compose(value)['layerWeights'],want)

    def test_outer_exit_has_default_only(self):
        for world in ([496.,0.,0.], [0.,240.,0.], [0.,0.,-497.]):
            value=parameters(worldPosition=world)
            result=self.compose(value)
            self.assertRows(result['coefficients'],value['defaultCoefficients'])
            self.assertFalse(result['volumeActive'])

    def test_coefficients_remain_signed_and_unclamped(self):
        value=parameters(defaultCoefficients=[[0.]*4]*3)
        value['sampledLayers'][0]['directionalRgba']=[[0.,1.,.5,.25]]*3
        self.assertRows(self.compose(value)['coefficients'],[[-2,2,0,1]]*3)

    def test_independent_colored_field_preserves_rgb_groups(self):
        value=parameters(defaultCoefficients=[[0.]*4]*3)
        value['sampledLayers'][0]=dict(amplitude=[1.,2.,3.],
            directionalRgba=[[1.,0.,.5,.5],[.5,1.,0.,.3],[0.,.5,1.,.9]])
        self.assertRows(self.compose(value)['coefficients'],[[2,-2,0,1],[0,4,-4,2],[-6,0,6,3]])

    def test_zero_amplitude_keeps_default_alpha_control(self):
        value=parameters()
        for layer in value['sampledLayers']:
            layer['amplitude']=[0.]*3
            for direction in layer['directionalRgba']:direction[3]=1.
        self.assertRows(self.compose(value)['coefficients'],[[1,6,3,5.5],[.5,3,1.5,2.75],[1.5,9,4.5,8.25]])

    def test_decode_preserves_exact_bytes_and_normalized_alpha(self):
        rgba=[255,128,0,17,0,255,128,99,128,0,255,213]
        raw=struct.pack('<I',0x781e03c0)+bytes(rgba)
        for source in (raw,bytearray(raw)):
            result=composition.decode_native_irradiance_sample(source)
            self.assertEqual(result['amplitude'],[1.,1.,1.])
            self.assertEqual(result['directionalRgba8'],rgba)
            self.assertEqual(result['directionalAlpha'],[17/255,99/255,213/255])
            self.assertEqual(result['directionalRgba'],[[x/255 for x in rgba[i:i+4]] for i in (0,4,8)])

    def test_decoded_sample_is_directly_accepted_by_composition(self):
        raw=struct.pack('<I',0x781e03c0)+bytes([128,128,128,255])*3
        decoded=composition.decode_native_irradiance_sample(raw)
        value=parameters(sampledLayers=[decoded]*3)
        result=self.compose(value)
        self.assertEqual(result['directionalAlpha'],[[1.,1.,1.]]*3)
        self.assertAlmostEqual(result['coefficients'][0][0],1+2/255)

    def test_decoder_preserves_independent_packed_amplitudes(self):
        raw=struct.pack('<I',0x842003c0)+bytes(12)
        result=composition.decode_native_irradiance_sample(raw)
        self.assertEqual(result['amplitude'],[1.,2.,3.])

    def test_optional_byte_and_alpha_metadata_must_match_samples(self):
        raw=struct.pack('<I',0x781e03c0)+bytes([128,128,128,255])*3
        decoded=composition.decode_native_irradiance_sample(raw)
        for field,value in (('directionalRgba8',[0]*12),('directionalRgba8',[True]*12),
                            ('directionalRgba8',[-1]*12),('directionalRgba8',[]),
                            ('directionalAlpha',[0.]*3),('directionalAlpha',None)):
            layer=copy.deepcopy(decoded);layer[field]=value
            with self.subTest(field=field,value=value),self.assertRaises(ValueError):
                self.compose(parameters(sampledLayers=[layer]*3))

    def test_inputs_and_outputs_have_separate_ownership(self):
        value=parameters();saved=copy.deepcopy(value)
        result=self.compose(value)
        result['coefficients'][0][0]=100.
        result['directionalAlpha'][0][0]=100.
        self.assertEqual(value,saved)

    def test_missing_parameters_unknown_fields_and_versions_are_value_errors(self):
        with self.assertRaises(ValueError):composition.compose_native_irradiance()
        for missing in parameters():
            value=parameters();del value[missing]
            with self.subTest(missing=missing),self.assertRaises(ValueError):self.compose(value)
        for version in (True,0,2,1.,'1',None):
            with self.subTest(version=version),self.assertRaises(ValueError):self.compose(parameters(version=version))
        value=parameters();value['unspecified']=1
        with self.assertRaises(ValueError):self.compose(value)

    def test_non_mapping_wrong_shape_and_boolean_numbers_are_value_errors(self):
        for value in (None,1,[],[parameters()],True,'parameters'):
            with self.subTest(value=value),self.assertRaises(ValueError):self.compose(value)
        for key in ('worldPosition','cameraPosition','chunkOrigin','centerScales','defaultCoefficients','sampledLayers'):
            for invalid in (None,{},'000',[],[0,0]):
                with self.subTest(key=key,invalid=invalid),self.assertRaises(ValueError):self.compose(parameters(**{key:invalid}))
        for key in ('worldPosition','cameraPosition','chunkOrigin','centerScales'):
            with self.assertRaises(ValueError):self.compose(parameters(**{key:[True,0,0]}))
        for enabled in (1,0,'false',None):
            with self.assertRaises(ValueError):self.compose(parameters(enabled=enabled))

    def test_nonfinite_values_and_arithmetic_overflow_are_value_errors(self):
        for key in ('worldPosition','cameraPosition','chunkOrigin','centerScales'):
            for invalid in (math.nan,math.inf,-math.inf,10**500):
                with self.subTest(key=key,invalid=str(invalid)[:20]),self.assertRaises(ValueError):self.compose(parameters(**{key:[invalid,0,0]}))
        for field in ('amplitude','directionalRgba'):
            value=parameters()
            if field=='amplitude':value['sampledLayers'][2][field][0]=math.nan
            else:value['sampledLayers'][2][field][0][0]=math.inf
            with self.assertRaises(ValueError):self.compose(value)
        value=parameters();value['defaultCoefficients'][0][0]=math.nan
        with self.assertRaises(ValueError):self.compose(value)
        with self.assertRaises(ValueError):self.compose(parameters(cameraPosition=[1e308,0,0],centerScales=[1e308,4,16]))
        value=parameters();value['sampledLayers'][0]['amplitude']=[1e308]*3
        with self.assertRaises(ValueError):self.compose(value)

    def test_layers_reject_missing_unknown_invalid_and_out_of_range_data(self):
        for bad in (None,[],dict(amplitude=[1,1,1]),dict(directionalRgba=[[0]*4]*3)):
            value=parameters();value['sampledLayers'][0]=bad
            with self.assertRaises(ValueError):self.compose(value)
        for bad in (-.01,1.01,True,'0',None):
            value=parameters();value['sampledLayers'][0]['directionalRgba'][0][3]=bad
            with self.assertRaises(ValueError):self.compose(value)
        for bad in (-1,True,'1',None,[],[0,0]):
            value=parameters();value['sampledLayers'][0]['amplitude'][0]=bad
            with self.assertRaises(ValueError):self.compose(value)
        for key,bad in (('amplitude',[]),('directionalRgba',[]),('directionalRgba',[[0]*3]*3)):
            value=parameters();value['sampledLayers'][0][key]=bad
            with self.assertRaises(ValueError):self.compose(value)
        value=parameters();value['sampledLayers'][0]['unknown']=0
        with self.assertRaises(ValueError):self.compose(value)

    def test_disabled_still_requires_complete_valid_layers_and_globals(self):
        value=parameters(enabled=False);value['sampledLayers'][2]['directionalRgba'][2][3]=math.nan
        with self.assertRaises(ValueError):self.compose(value)
        value=parameters(enabled=False);del value['defaultCoefficients']
        with self.assertRaises(ValueError):self.compose(value)

    def test_decoder_rejects_missing_wrong_type_length_and_nonfinite_amplitude(self):
        with self.assertRaises(ValueError):composition.decode_native_irradiance_sample()
        for raw in (None,'0'*16,bytes(15),bytes(17),[0]*16,struct.pack('<I',0xffffffff)+bytes(12)):
            with self.subTest(raw=repr(raw)),self.assertRaises(ValueError):composition.decode_native_irradiance_sample(raw)


if __name__=='__main__':unittest.main()
