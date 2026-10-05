"""Exercise the dependency checker on independent CMake and include fixtures."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECKER = Path(__file__).with_name('check_module_boundaries.py')

class BoundaryTests(unittest.TestCase):
    def run_case(self, runtime_include='', editor_include='', dependency='', host_include=None):
        with tempfile.TemporaryDirectory(prefix='azure-boundary-') as temporary:
            root = Path(temporary)
            for module in ['runtime', 'editor', 'gameplay']:
                (root / 'src' / module).mkdir(parents=True)
            (root / 'src/runtime/Core.cpp').write_text(runtime_include, encoding='utf-8')
            (root / 'src/runtime/Core.hpp').write_text('#pragma once\n', encoding='utf-8')
            (root / 'src/editor/Panel.cpp').write_text(editor_include, encoding='utf-8')
            (root / 'src/editor/Panel.hpp').write_text('#pragma once\n', encoding='utf-8')
            (root / 'src/gameplay/Mechanism.hpp').write_text('#pragma once\n', encoding='utf-8')
            (root / 'src/gameplay/Mechanism.cpp').write_text('#include "runtime/Core.hpp"\n', encoding='utf-8')
            reply = root / 'build/.cmake/api/v1/reply'
            reply.mkdir(parents=True)
            targets = [
                {'name':'AzureRuntime', 'id':'runtime', 'sources':[{'path':'src/runtime/Core.cpp'}], 'dependencies':[{'id':dependency}] if dependency else []},
                {'name':'AzureEditor', 'id':'editor', 'sources':[{'path':'src/editor/Panel.cpp'}], 'dependencies':[{'id':'runtime'}]},
                {'name':'AzureGameplay', 'id':'gameplay', 'sources':[{'path':'src/gameplay/Mechanism.cpp'}], 'dependencies':[{'id':'runtime'}]},
            ]
            if host_include is not None:
                (root/'src/player').mkdir()
                (root/'src/player/Host.cpp').write_text(host_include,encoding='utf-8')
                targets.append({'name':'AzurePlayerHost','id':'host','sources':[{'path':'src/player/Host.cpp'}],
                                'compileGroups':[{'defines':[{'define':'AZURE_WITH_EDITOR=0'}]}]})
            for target in targets:
                (reply / (target['id'] + '.json')).write_text(json.dumps(target), encoding='utf-8')
            model = {'paths':{'source':str(root), 'build':str(root/'build')}, 'configurations':[{'targets':[
                {'name':t['name'], 'id':t['id'], 'jsonFile':t['id']+'.json'} for t in targets]}]}
            (reply/'codemodel-v2.json').write_text(json.dumps(model), encoding='utf-8')
            return subprocess.run([sys.executable,str(CHECKER),'--source',str(root),'--build-dir',str(root/'build')],capture_output=True,text=True)

    def test_allowed_editor_dependency(self):
        result = self.run_case(editor_include='#include "runtime/Core.hpp"\n')
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_runtime_cannot_include_editor(self):
        result = self.run_case(runtime_include='#include "editor/Panel.hpp"\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('AzureRuntime', result.stdout)
        self.assertIn('AzureEditor', result.stdout)

    def test_reverse_target_edge_rejected(self):
        result = self.run_case(dependency='editor')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('dependency', result.stdout)

    def test_runtime_cannot_include_shared_mechanism(self):
        result = self.run_case(runtime_include='#include "gameplay/Mechanism.hpp"\n')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('AzureGameplay', result.stdout)

    def test_runtime_cannot_link_shared_mechanism(self):
        result = self.run_case(dependency='gameplay')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('dependency', result.stdout)

    def test_include_cycles_do_not_hide_violation(self):
        result = self.run_case(runtime_include='#include "Core.hpp"\n#include "editor/Panel.hpp"\n',
                               editor_include='#include "runtime/Core.hpp"\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('include', result.stdout)

    def test_player_uses_effective_compile_definitions(self):
        result = self.run_case(host_include='#if AZURE_WITH_EDITOR\n#include "editor/Panel.hpp"\n#endif\n')
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_player_unconditional_editor_include_rejected(self):
        result = self.run_case(host_include='#include "editor/Panel.hpp"\n')
        self.assertNotEqual(result.returncode,0)
        self.assertIn('AzurePlayerHost',result.stdout)

if __name__ == '__main__':
    unittest.main()
