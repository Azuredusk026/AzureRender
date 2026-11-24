"""Analytic fixtures for the source-backed projected hair ramp contract."""
import math
import unittest

try:
    from endfield_native import projected_hair as subject
except ImportError:
    subject = None


class ProjectedHairMathTests(unittest.TestCase):
    def test_captured_visible_primary_and_secondary_lobes(self):
        # Official frame 10401, event 950: upright pixels (1258,215),
        # (1268,212), (1309,218). Expected values precede illumination.
        samples=[
            ((.9051864743232727,.5314453840255737),.7290030121803284,.7780070304870605,
             (.65059894323349,.6287327408790588,.7834439873695374),1.,1.,
             (.11162308976054192,.1078715082257986,.13441529124975204),(.11208641529083252,0.,.00009252656309399754)),
            ((.8807920217514038,.8428359031677246),.9180609583854675,.7585858106613159,
             (1.,1.,1.),.9857480525970459,.992874026298523,
             (.20724505186080933,)*3,(.05414295196533203,0.,.00004469463601708412)),
            ((.8684737682342529,.7333201169967651),.8563411235809326,.7443068027496338,
             (.9743495583534241,.9734492897987366,.9812924265861511),.9957732558250427,1.,
             (.18760830163955688,.18743498250842094,.18894515931606293),(.07046377658843994,0.,.00005816736666020006))]
        for uv,edge,lobe,ramp,parameter_g,parameter_a,primary,secondary in samples:
            with self.subTest(uv=uv):
                actual=subject.evaluate_energy(subject.Coordinates(uv,edge,lobe),ramp,
                    (0,parameter_g,0,parameter_a),1.2999999523162842,(.4091444674721517,0,.00033774597523219814))
                self.assertLess(max(abs(a-b) for a,b in zip(actual.primary,primary)),3e-8)
                self.assertLess(max(abs(a-b) for a,b in zip(actual.secondary,secondary)),3e-8)
                self.assertGreater(min(actual.primary),.1)

    def test_reflected_frame_preserves_fractional_strand_blend(self):
        args=dict(half_vector=(.1,.2,.97),highlight_normal=(.3,.2,.93),view=(.1,-.1,.98),
            source_parameters=(.37,.8,0,.4),tangent=(.95,.1,-.3,1),
            object_x=(1,0,0),object_y=(0,1,0),object_z=(0,0,1),
            primary_shift=.16,secondary_shift=-.2,range2=.7,edge_fade_power=2,direction_x=.2)
        expected=subject.project_coordinates(**args)
        reflected=dict(args)
        for field in ('half_vector','highlight_normal','view','object_x','object_y','object_z'):
            value=args[field];reflected[field]=(-value[0],*value[1:])
        reflected['tangent']=(-args['tangent'][0],*args['tangent'][1:3],-args['tangent'][3])
        actual=subject.project_coordinates(**reflected)
        self.assertVectorAlmostEqual(actual.uv,expected.uv)
        self.assertAlmostEqual(actual.secondary_lobe,expected.secondary_lobe)

    def test_explicit_tangent_sign_recovers_captured_normal(self):
        actual=subject.project_normal(encoded=(.177737087,.240192235),scale=1,
            normal=(-.810732484,-.348598927,.466073692),
            tangent=(.583349943,-.538139224,.595795393,-1),tangent_sign=-1)
        expected=(-.8112095,.545542955,.210528269)
        self.assertLess(max(abs(a-b) for a,b in zip(actual,expected)),2e-7)
    def test_line_modulates_indirect_and_protects_primary_band(self):
        args=dict(strand=(0,1,0),normal=(0,0,1),half_vector=(1,0,0),
            u=.007,frequency=100,strand_shift=0,range1=.95,depth=.5,
            alternation=1,line_sample=.8,parameter_g=.5,q=(0,0,0),saturation=1.5)
        factor,saturation=subject.line_response(**args)
        self.assertAlmostEqual(factor,.8);self.assertAlmostEqual(saturation,1.1)
        args['q']=(1,.1,.2)
        self.assertVectorAlmostEqual(subject.line_response(**args),(1,1))
        args.update(q=(0,0,0),alternation=0,u=.005)
        self.assertAlmostEqual(subject.line_response(**args)[0],.75)
        args['u']=.00501
        self.assertAlmostEqual(subject.line_response(**args)[0],1)

    def test_virtual_main_half_uses_projected_view_and_object_axes(self):
        value=subject.virtual_half_vector(view=(0,0,1),light=(0,1,0),
            object_x=(1,0,0),object_y=(0,1,0),object_z=(0,0,1),blend=1)
        length=math.sqrt(13);y=3/length;z=1+2/length;normal=math.sqrt(y*y+z*z)
        self.assertVectorAlmostEqual(value,(0,y/normal,z/normal))
        self.assertGreater(abs(value[1]-math.sqrt(.5)),.1)
    def setUp(self):
        self.assertIsNotNone(subject, 'Projected hair reference contract is missing')

    def coordinates(self, **updates):
        args = dict(half_vector=(1, 0, 0), highlight_normal=(0, 0, 1),
            view=(0, 0, 1), source_parameters=(1, .5, 0, .4),
            tangent=(1, 0, 0, 1), object_x=(1, 0, 0),
            object_y=(0, 1, 0), object_z=(0, 0, 1),
            primary_shift=0, secondary_shift=0, range2=.5,
            edge_fade_power=2, direction_x=0)
        args.update(updates)
        return subject.project_coordinates(**args)

    def assertVectorAlmostEqual(self, actual, expected, places=12):
        for a, e in zip(actual, expected):
            self.assertAlmostEqual(a, e, places=places)

    def test_two_dimensional_ramp_retains_sign_and_projected_edge(self):
        root = math.sqrt(.5)
        front = self.coordinates(half_vector=(root, root, 0),
            view=(math.sqrt(.75), 0, .5), source_parameters=(1, 1, 0, .4))
        back = self.coordinates(half_vector=(root, -root, 0),
            view=(math.sqrt(.75), 0, .5), source_parameters=(1, 1, 0, .4))
        self.assertAlmostEqual(front.edge_fade, .25)
        self.assertAlmostEqual(front.uv[0] / 2**-100, 1, places=11)
        self.assertAlmostEqual(front.uv[1], .0625)
        self.assertAlmostEqual(back.uv[0] / 2**-100, 1, places=11)
        self.assertEqual(back.uv[1], 0)

    def test_channels_keep_primary_and_secondary_energy_independent(self):
        coordinates = self.coordinates()
        energy = subject.evaluate_energy(coordinates, (1, .5, .25),
            (1, .5, 0, .4), 2, (.2, .4, .8))
        self.assertVectorAlmostEqual(energy.q, (.5, .25, .125))
        self.assertVectorAlmostEqual(energy.primary, (.1, .05, .025))
        self.assertVectorAlmostEqual(energy.secondary, (.04, .08, .16))
        empty = subject.evaluate_energy(self.coordinates(source_parameters=(1, 0, 0, .4)),
            (1, .5, .25), (1, 0, 0, .4), 2, (.2, .4, .8))
        self.assertVectorAlmostEqual(empty.primary, (0, 0, 0))
        self.assertVectorAlmostEqual(empty.secondary, (.08, .16, .32))
        no_secondary = subject.evaluate_energy(coordinates, (1, .5, .25),
            (1, .5, 0, 0), 2, (.2, .4, .8))
        self.assertVectorAlmostEqual(no_secondary.secondary, (0, 0, 0))

    def test_parameter_r_and_tangent_handedness_change_strand_sign(self):
        half = (math.sqrt(.5), math.sqrt(.5), 0)
        object_axis = self.coordinates(half_vector=half,
            source_parameters=(0, 1, 0, 1))
        mesh_axis = self.coordinates(half_vector=half,
            source_parameters=(1, 1, 0, 1))
        mirrored = self.coordinates(half_vector=half,
            source_parameters=(1, 1, 0, 1), tangent=(1, 0, 0, -1))
        self.assertEqual(object_axis.uv[1], 0)
        self.assertEqual(mesh_axis.uv[1], 1)
        self.assertEqual(mirrored.uv[1], 0)
        self.assertAlmostEqual(mesh_axis.uv[0] / 2**-100, 1, places=11)

    def test_secondary_range_uses_integer_exponent_and_signed_shift(self):
        half = (math.sqrt(.5), math.sqrt(.5), 0)
        truncated = self.coordinates(half_vector=half, range2=.496)
        self.assertAlmostEqual(truncated.secondary_lobe / 2**-50, 1, places=11)
        broad = self.coordinates(half_vector=half, range2=1.5)
        self.assertEqual(broad.secondary_lobe, 1)
        shifted = self.coordinates(half_vector=(0, 0, 1), primary_shift=1,
            secondary_shift=-1, source_parameters=(1, 1, 0, 1), range2=.992)
        self.assertAlmostEqual(shifted.uv[0] / 2**-100, 1, places=11)
        self.assertEqual(shifted.uv[1], 1)
        self.assertAlmostEqual(shifted.secondary_lobe, math.sqrt(.5))

    def test_object_basis_rotation_preserves_coordinates(self):
        root = math.sqrt(.5)
        baseline = self.coordinates(half_vector=(root, root, 0),
            view=(math.sqrt(.75), 0, .5))
        rotated = self.coordinates(half_vector=(0, root, -root),
            highlight_normal=(1, 0, 0), view=(.5, 0, -math.sqrt(.75)),
            tangent=(0, 0, -1, 1), object_x=(0, 0, -1),
            object_y=(0, 1, 0), object_z=(1, 0, 0))
        self.assertVectorAlmostEqual(rotated.uv, baseline.uv)
        self.assertAlmostEqual(rotated.edge_fade, baseline.edge_fade)
        self.assertAlmostEqual(rotated.secondary_lobe, baseline.secondary_lobe)

    def test_degenerate_projection_and_strand_are_finite(self):
        projected = self.coordinates(view=(0, 1, 0))
        self.assertEqual(projected.edge_fade, 0)
        self.assertEqual(projected.uv[1], 0)
        cancelled = self.coordinates(source_parameters=(.5, 1, 0, 1),
            half_vector=(0, 0, 0), highlight_normal=(0, 0, 0),
            object_x=(0, 0, 0), object_y=(0, 0, 0), object_z=(0, 0, 0))
        for value in (*cancelled.uv, cancelled.edge_fade, cancelled.secondary_lobe):
            self.assertTrue(math.isfinite(value))


if __name__ == '__main__':
    unittest.main()
