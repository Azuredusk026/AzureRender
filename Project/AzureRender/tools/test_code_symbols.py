"""Check real Tree-sitter parsing, incremental freshness and bounded maps."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).with_name('code_query.py')


class SymbolTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(['git','init','-q',str(self.root)],check=True)
        self.write('.gitignore','build/\n')
        self.write('src/api.hpp','namespace demo {\nstruct Widget {\n int draw(int x);\n int draw(float x);\n};\nusing MaterialDocument = Widget;\n}\n')
        self.write('src/main.cpp','#include "api.hpp"\nint demo::Widget::draw(int x) { return x; }\n')

    def write(self,name,data):
        p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data,encoding='utf-8');return p

    def query(self,operation,query,*args):
        result=subprocess.run([sys.executable,str(TOOL),'--root',str(self.root),operation,query,*args,'--json'],capture_output=True,encoding='utf-8')
        self.assertTrue(result.stdout.strip(),result.stderr)
        return result.returncode,json.loads(result.stdout),result.stdout

    def test_cpp_overloads_namespaces_aliases_and_definition_ranges(self):
        code,data,_=self.query('symbols','draw')
        self.assertEqual(code,0)
        self.assertEqual(len(data['results']),3)
        self.assertEqual([r['line'] for r in data['results'] if r['path']=='src/api.hpp'],[3,4])
        self.assertTrue(all('draw' in r['qualifiedName'] for r in data['results']))
        self.assertEqual(self.query('symbols','MaterialDocument')[1]['results'][0]['path'],'src/api.hpp')

    def test_unchanged_query_reuses_index_and_content_hash_detects_same_timestamp_edit(self):
        first=self.query('symbols','Widget')[1]
        second=self.query('symbols','Widget')[1]
        self.assertGreater(first['index']['parsedFiles'],0)
        self.assertEqual(second['index']['parsedFiles'],0)
        self.assertTrue(second['index']['cacheHit'])
        p=self.root/'src/api.hpp';stamp=p.stat().st_mtime_ns
        self.write('src/api.hpp',p.read_text(encoding='utf-8').replace('Widget','Gadget'));os.utime(p,ns=(stamp,stamp))
        code,changed,_=self.query('symbols','Gadget')
        self.assertEqual(code,0)
        self.assertEqual(changed['index']['parsedFiles'],1)
        self.assertNotEqual(first['index']['fingerprint'],changed['index']['fingerprint'])
        self.assertEqual(changed['results'][0]['sha256'],hashlib.sha256(p.read_bytes()).hexdigest())

    def test_rename_delete_and_untracked_symbols_update(self):
        self.query('symbols','Widget')
        (self.root/'src/api.hpp').rename(self.root/'src/moved.hpp')
        data=self.query('symbols','Widget')[1]
        self.assertTrue(any(r['path']=='src/moved.hpp' for r in data['results']))
        self.assertFalse(any(r['path']=='src/api.hpp' for r in data['results']))
        (self.root/'src/moved.hpp').unlink()
        self.assertEqual(self.query('symbols','MaterialDocument')[0],1)

    def test_python_and_csharp_methods_are_structural_symbols(self):
        self.write('src/example.py','class Tool:\n    def execute(self):\n        return 1\n')
        self.write('managed/Example.cs','class Tool { public int Execute() { return 1; } }')
        data=self.query('symbols','execute')[1]
        self.assertEqual({r['path'] for r in data['results']},{'src/example.py','managed/Example.cs'})

    def test_preprocessor_branches_and_templates_are_explicitly_syntactic(self):
        self.write('src/config.hpp','#if DEBUG\nstruct DebugOnly {};\n#else\nstruct ReleaseOnly {};\n#endif\ntemplate<class T> struct Holder {};\n#define REGISTER(x) x\nREGISTER(Widget)\n')
        self.assertEqual(self.query('symbols','Only')[0],0)
        data=self.query('symbols','Holder')[1]
        self.assertEqual(data['index']['precision'],'syntax')
        self.assertFalse(data['index']['preprocessorEvaluated'])

    def test_map_follows_includes_with_depth_budget_and_current_hashes(self):
        code,data,raw=self.query('map','Widget','--depth','1','--max-chars','1800')
        self.assertEqual(code,0)
        self.assertLessEqual(len(raw),1800)
        self.assertEqual({r['path'] for r in data['results']},{'src/api.hpp','src/main.cpp'})
        self.assertTrue(all(r['sha256'] for r in data['results']))
        self.assertEqual(self.query('map','Widget','--depth','5')[0],2)

    def test_module_scope_and_symbol_budget_are_enforced(self):
        self.write('src/other/hidden.hpp','struct WidgetHidden {};')
        code,data,raw=self.query('symbols','Widget','--module','src/other','--max-chars','1000')
        self.assertEqual(code,0)
        self.assertEqual([r['path'] for r in data['results']],['src/other/hidden.hpp'])
        self.assertLessEqual(len(raw),1000)

    def test_corrupt_index_is_rebuilt(self):
        self.query('symbols','Widget')
        for p in (self.root/'build/code-intelligence').glob('*.json'):p.write_text('broken')
        code,data,_=self.query('symbols','Widget')
        self.assertEqual(code,0)
        self.assertGreater(data['index']['parsedFiles'],0)

    def test_extractor_change_invalidates_cached_symbols(self):
        self.query('symbols','Widget')
        for p in (self.root/'build/code-intelligence').glob('*.json'):
            data=json.loads(p.read_text(encoding='utf-8'));data['implementation']='older-version';p.write_text(json.dumps(data))
        self.assertGreater(self.query('symbols','Widget')[1]['index']['parsedFiles'],0)

    def test_missing_native_backend_has_structured_error_and_text_fallback(self):
        from contextlib import redirect_stdout
        import io
        from unittest.mock import patch
        import code_query
        with patch.dict(sys.modules,{'tree_sitter':None}):
            output=io.StringIO()
            with redirect_stdout(output):code=code_query.main(['--root',str(self.root),'symbols','Widget'])
            self.assertEqual(code,2)
            self.assertIn('Install',json.loads(output.getvalue())['error'])
            output=io.StringIO()
            with redirect_stdout(output):code=code_query.main(['--root',str(self.root),'search','Widget'])
            self.assertEqual(code,0)
            self.assertTrue(json.loads(output.getvalue())['results'])


if __name__ == '__main__':unittest.main()
