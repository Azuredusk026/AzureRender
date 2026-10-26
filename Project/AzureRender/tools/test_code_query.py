"""Exercise bounded queries against real Git repositories and ripgrep."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOL = Path(__file__).with_name('code_query.py')


class QueryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        self.write('.gitignore', 'build/\nassets_private/\n*.ignored\n')
        self.write('src/a.cpp', 'int RenderGraph = 1;\n// 中文定义\n')
        self.write('shaders/a.frag', '// RenderGraph\n')
        self.write('tests/a.cpp', '// RenderGraph\n')
        self.write('docs/acceptance/source-snapshot/a.cpp', 'RenderGraph\n')
        self.write('docs/runtime/graph.md', '# RenderGraph\n')
        self.write('build/secret.cpp', 'RenderGraph private\n')
        self.write('assets_private/secret.cpp', 'RenderGraph private\n')
        subprocess.run(['git', '-C', str(self.root), 'add', '.'], check=True)

    def write(self, name, text):
        p = self.root / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding='utf-8')
        return p

    def query(self, *args):
        result = subprocess.run([sys.executable, str(TOOL), '--root', str(self.root), *args, '--json'],
                                capture_output=True, encoding='utf-8')
        self.assertTrue(result.stdout.strip(), result.stderr)
        return result.returncode, json.loads(result.stdout), result.stdout

    def test_source_excludes_evidence_tests_build_and_private(self):
        code, data, _ = self.query('search', 'RenderGraph')
        self.assertEqual(code, 0)
        self.assertEqual([r['path'] for r in data['results']], ['shaders/a.frag', 'src/a.cpp'])
        self.assertEqual([r['line'] for r in data['results']], [1, 1])

    def test_untracked_code_is_found_and_module_limits_directory(self):
        self.write('src/render/new.cpp', '// needle\n')
        self.write('src/editor/new.cpp', '// needle\n')
        code, data, _ = self.query('search', 'needle', '--module', 'src/render')
        self.assertEqual(code, 0)
        self.assertEqual([r['path'] for r in data['results']], ['src/render/new.cpp'])

    def test_evidence_and_ignored_files_need_explicit_scope(self):
        code, data, _ = self.query('search', 'RenderGraph', '--scope', 'evidence')
        self.assertEqual(code, 0)
        self.assertEqual(data['results'][0]['path'], 'docs/acceptance/source-snapshot/a.cpp')
        self.assertEqual(self.query('read', 'build/secret.cpp')[0], 2)
        code, data, _ = self.query('read', 'build/secret.cpp', '--scope', 'artifacts')
        self.assertEqual(code, 0)
        self.assertIn('private', data['results'][0]['lines'][0])

    def test_literal_query_has_no_regex_or_shell_interpretation(self):
        self.write('src/literal.cpp', '// $x.*[0]; RenderGraph\n')
        code, data, _ = self.query('search', '$x.*[0]')
        self.assertEqual(code, 0)
        self.assertEqual(data['results'][0]['path'], 'src/literal.cpp')

    def test_output_cap_counts_complete_unicode_json_and_marks_truncation(self):
        self.write('src/many.cpp', ('// needle 中文' + '很长' * 200 + '\n') * 50)
        code, data, raw = self.query('search', 'needle', '--max-chars', '1400')
        self.assertEqual(code, 0)
        self.assertLessEqual(len(raw), 1400)
        self.assertLessEqual(len(data['results']), 20)
        self.assertTrue(data['truncated'])
        self.assertTrue(data['suggestion'])

    def test_read_line_numbers_eof_and_fingerprint_follow_current_file(self):
        code, data, _ = self.query('read', 'src/a.cpp', '--start', '2', '--lines', '80')
        self.assertEqual(code, 0)
        row = data['results'][0]
        self.assertEqual((row['start'], row['end'], row['lines']), (2, 2, ['// 中文定义']))
        self.assertEqual(row['sha256'], hashlib.sha256((self.root/'src/a.cpp').read_bytes()).hexdigest())
        previous = row['sha256']
        self.write('src/a.cpp', '// changed\n')
        self.assertNotEqual(previous, self.query('read', 'src/a.cpp')[1]['results'][0]['sha256'])

    def test_no_hit_and_invalid_requests_are_distinct(self):
        self.assertEqual(self.query('search', 'not-here')[0], 1)
        self.assertEqual(self.query('read', '../escape.cpp')[0], 2)
        self.assertEqual(self.query('read', 'src/a.cpp', '--start', '0')[0], 2)
        self.assertEqual(self.query('search', 'x', '--module', 'docs')[0], 2)
        self.assertEqual(self.query('search', 'x', '--scope', 'unknown')[0], 2)

    def test_directory_junction_is_rejected(self):
        outside = self.root/'outside'
        outside.mkdir()
        (outside/'secret.cpp').write_text('secret')
        link = self.root/'src/link'
        if os.name == 'nt':
            import _winapi
            _winapi.CreateJunction(str(outside), str(link))
        else:
            link.symlink_to(outside, target_is_directory=True)
        self.addCleanup(lambda: os.rmdir(link) if os.name == 'nt' else link.unlink())
        self.assertEqual(self.query('read', 'src/link/secret.cpp')[0], 2)

    def test_timeout_returns_bounded_structured_error(self):
        code, data, raw = self.query('search', 'RenderGraph', '--timeout', '0.000001')
        self.assertEqual(code, 2)
        self.assertEqual(data['status'], 'error')
        self.assertLessEqual(len(raw), 12000)

    def test_truncation_hint_is_included_in_json_budget(self):
        from code_query import bounded_output
        payload = {'results': [{'text': 'x' * 630}], 'truncated': True}
        raw = bounded_output(payload, 700)
        self.assertLessEqual(len(raw), 700)
        self.assertTrue(json.loads(raw)['suggestion'])

    def test_read_long_line_is_explicitly_partial(self):
        self.write('src/long.cpp', '中' * 30000)
        code, data, raw = self.query('read', 'src/long.cpp', '--max-chars', '1000')
        self.assertEqual(code, 0)
        self.assertLessEqual(len(raw), 1000)
        self.assertTrue(data['truncated'])
        self.assertTrue(data['results'][0]['partialLine'])


if __name__ == '__main__':
    unittest.main()
