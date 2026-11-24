"""Reject stale executables and freeze only production dependencies."""
import json
from pathlib import Path
import tempfile
import unittest
import build_provenance
from run_playable_performance import freeze_products


class PerformanceProvenanceTests(unittest.TestCase):
    def fixture(self, root):
        source = root/'source'; source.mkdir()
        (source/'src').mkdir(); (source/'src/main.cpp').write_text('original')
        build = root/'build'; build.mkdir()
        player = build/'AzurePlayer.exe'; player.write_bytes(b'player')
        editor = build/'AzureRender.exe'; editor.write_bytes(b'editor')
        (build/'dependency.dll').write_bytes(b'dependency')
        (build/'shaders').mkdir(); (build/'shaders/main.spv').write_bytes(b'shader')
        manifest = build_provenance.describe(source, [player, editor])
        (build/'build-provenance.json').write_text(json.dumps(manifest))
        return source, build, player, editor

    def test_unrelated_archives_excluded_but_dependencies_frozen(self):
        with tempfile.TemporaryDirectory() as temp:
            source, build, player, editor = self.fixture(Path(temp))
            archive=build/'old-package'; archive.mkdir()
            (archive/'dependency.dll').write_bytes(b'historical')
            (archive/'manifest.json').write_text('{}')
            products=freeze_products(source, player, editor)
            self.assertEqual(set(products), {str(p.resolve()) for p in
                (player,editor,build/'dependency.dll',build/'shaders/main.spv')})
            before=products[str((build/'dependency.dll').resolve())]
            (build/'dependency.dll').write_bytes(b'changed')
            self.assertNotEqual(freeze_products(source,player,editor)[str((build/'dependency.dll').resolve())],before)

    def test_stale_source_and_executable_refused_before_sampling(self):
        with tempfile.TemporaryDirectory() as temp:
            source, build, player, editor = self.fixture(Path(temp))
            (source/'src/main.cpp').write_text('changed')
            with self.assertRaisesRegex(ValueError,'source'):
                freeze_products(source,player,editor)
            (source/'src/main.cpp').write_text('original')
            player.write_bytes(b'stale')
            with self.assertRaisesRegex(ValueError,'product'):
                freeze_products(source,player,editor)

    def test_unrecorded_executable_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            source,build,player,editor=self.fixture(Path(temp))
            other=build/'Unrecorded.exe'; other.write_bytes(b'other')
            with self.assertRaisesRegex(ValueError,'recorded'):
                freeze_products(source,other,editor)


if __name__=='__main__':unittest.main()
