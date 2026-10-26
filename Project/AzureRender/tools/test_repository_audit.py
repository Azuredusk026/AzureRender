"""Exercise repository hygiene against real Git indexes and Markdown links."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).with_name('audit_repository.py')


class RepositoryAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        (self.root / '.gitignore').write_text('**/build/\n/.codemaker/\n.env\n', encoding='utf-8')
        (self.root / 'README.md').write_text('[Guide](Project/AzureRender/docs/index.md)\n', encoding='utf-8')
        guide = self.root / 'Project/AzureRender/docs/index.md'
        guide.parent.mkdir(parents=True)
        guide.write_text('# Guide\n[Web](https://example.com)\n', encoding='utf-8')
        subprocess.run(['git', '-C', str(self.root), 'add', '.'], check=True)

    def audit(self):
        output = self.root / 'result.json'
        result = subprocess.run([sys.executable, str(TOOL), '--source', str(self.root), '--output', str(output)],
            capture_output=True, encoding='utf-8')
        self.assertTrue(output.exists(), 'Audit must produce a machine-readable report: ' + result.stderr)
        return result.returncode, json.loads(output.read_text(encoding='utf-8'))

    def test_clean_repository_and_valid_relative_links_pass(self):
        code, report = self.audit()
        self.assertEqual(code, 0)
        self.assertEqual(report['status'], 'passed')
        self.assertEqual(report['danglingLinks'], [])

    def test_tracked_cache_fails_even_when_gitignore_matches(self):
        cache = self.root / '.codemaker/codemap/crash.log'
        cache.parent.mkdir(parents=True)
        cache.write_text('local state', encoding='utf-8')
        subprocess.run(['git', '-C', str(self.root), 'add', '-f', str(cache)], check=True)
        code, report = self.audit()
        self.assertEqual(code, 1)
        self.assertEqual(report['trackedToolState'], ['.codemaker/codemap/crash.log'])

    def test_missing_active_document_link_fails(self):
        (self.root / 'README.md').write_text('[Missing](Project/AzureRender/docs/missing.md#start)', encoding='utf-8')
        code, report = self.audit()
        self.assertEqual(code, 1)
        self.assertEqual(report['danglingLinks'][0]['target'], 'Project/AzureRender/docs/missing.md#start')

    def test_ignored_source_file_fails_hygiene(self):
        with (self.root / '.gitignore').open('a', encoding='utf-8') as stream:
            stream.write('**/src/\n')
        code, report = self.audit()
        self.assertEqual(code, 1)
        self.assertFalse(next(p for p in report['ignoreProbes'] if p['path'].endswith('src/main.cpp'))['passed'])

    def test_inventory_finds_nested_document_script_navigation_and_json_references(self):
        from evidence_inventory import reference_graph
        engine = self.root/'Project/AzureRender'
        target = engine/'docs/acceptance/p3/raw.bin'
        target.parent.mkdir(parents=True)
        target.write_bytes(b'raw')
        samples = {
            'docs/runtime/nested/guide.md': '[Raw](../../acceptance/p3/raw.bin#data)',
            'tools/probe.py': 'source="docs/acceptance/p3/raw.bin"',
            'mkdocs.yml': '  - Raw: acceptance/p3/raw.bin',
            'docs/plans/manifest.json': '{"source":"../acceptance/p3/raw.bin"}',
        }
        for name,text in samples.items():
            p=engine/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
        rows=reference_graph(self.root,[target])
        self.assertEqual({r['document'] for r in rows}, {'Project/AzureRender/'+n for n in samples})
        self.assertTrue(all(r['target']=='Project/AzureRender/docs/acceptance/p3/raw.bin' for r in rows))

    def test_inventory_counts_hardlinks_once_and_rejects_outside_paths(self):
        from evidence_inventory import inspect_directory
        import os
        target=self.root/'evidence';target.mkdir()
        (target/'a.bin').write_bytes(b'123456789')
        os.link(target/'a.bin',target/'b.bin')
        data=inspect_directory(self.root,target)
        self.assertEqual((data['logicalBytes'],data['uniqueFileDataBytes'],data['files']),(18,9,2))
        with self.assertRaises(ValueError):inspect_directory(target,self.root)

    def test_reference_literals_that_look_like_invalid_urls_are_ignored(self):
        from evidence_inventory import reference_graph
        document=self.root/'regex.py';document.write_text('pattern="//[0]/not-a-host"')
        self.assertEqual(reference_graph(self.root,[self.root/'target']),[])


if __name__ == '__main__':
    unittest.main()
