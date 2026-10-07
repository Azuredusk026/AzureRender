"""Exercise coverage integrity with real Git history and archived bytes."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from verify_evolution_coverage import verify

ITEMS=[*(f'A{i}' for i in range(1,7)),*(f'B{i}' for i in range(1,9)),*(f'C{i}' for i in range(1,5))]
class CoverageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.source=self.root/'Project/Engine';self.source.mkdir(parents=True)
        self.git('init','-q');self.git('config','user.name','Coverage fixture');self.git('config','user.email','coverage@example.invalid')
        header=self.source/'src/Contract.hpp';header.parent.mkdir();header.write_bytes(b'struct Contract {};\n')
        self.git('add','.');self.git('commit','-qm','base');parent=self.git('rev-parse','HEAD').strip()
        self.archive=self.source/'docs/acceptance/f4/evidence';self.archive.mkdir(parents=True)
        self.raw=self.archive/'tests.log';self.raw.write_bytes(b'100% tests passed, 0 tests failed out of 1\n')
        self.receipt={'schemaVersion':1,'phase':'F4','status':'passed','parentCommit':parent,'sourceItems':ITEMS,
            'referenceCommit':parent,'tests':{'Debug':1,'Release':1},
            'files':{'tests.log':hashlib.sha256(self.raw.read_bytes()).hexdigest()},
            'implementationInputs':{'Project/Engine/src/Contract.hpp':hashlib.sha256(b'struct Contract {};\r\n').hexdigest()}}
        self.saveReceipt()
        acceptance=self.source/'docs/acceptance/f4/acceptance.md';acceptance.write_text('accepted\n',encoding='utf-8')
        self.git('add','.');self.git('commit','-qm','feat(f4): fixture')
        self.commit=self.git('rev-parse','HEAD').strip()
        verification={'acceptance':'docs/acceptance/f4/acceptance.md','evidence':'docs/acceptance/f4/evidence/manifest.json',
            'adoption':'adapted-adoption','commit':self.commit}
        self.manifest={'schemaVersion':1,'referenceCommit':parent,'adoptionOutcomes':['adapted-adoption'],
            'phases':[{'id':'F4','tasks':['F4.1'],'items':ITEMS,'verification':verification}],
            'tasks':[{'id':'F4.1'}],
            'items':[{'id':item,'tasks':['F4.1'],'phases':['F4'],'finalClosure':'P3.1',
                'implementation':{'interfaces':['Contract'],'interfaceFiles':['src/Contract.hpp'],
                    'tests':['Contract.Test'],'verification':copy.deepcopy(verification),'scope':'Fixture public contract',
                    'riskResolution':{'tests':['Contract.Test'],'limits':['Fixture only']}}} for item in ITEMS]}
        self.discovery={'tests':[{'name':'Contract.Test'}]}
    def git(self,*args):
        return subprocess.check_output(['git','-C',str(self.root),*args],text=True,encoding='utf-8').strip()
    def saveReceipt(self):
        (self.archive/'manifest.json').write_text(json.dumps(self.receipt),encoding='utf-8')
    def check(self):return verify(self.source,self.manifest,self.discovery)
    def test_committed_history_accepts_crlf_and_later_source_edits(self):
        (self.source/'src/Contract.hpp').write_text('struct Contract { int extension; };\n',encoding='utf-8')
        self.git('add','.');self.git('commit','-qm','later extension')
        self.assertEqual(self.check()['coveredItems'],18)
    def test_missing_item_is_rejected(self):
        self.manifest['items'].pop()
        with self.assertRaisesRegex(ValueError,'18'):self.check()
    def test_unmapped_item_is_rejected(self):
        del self.manifest['items'][0]['implementation']
        with self.assertRaisesRegex(ValueError,'implementation'):self.check()
    def test_archived_bytes_tampering_is_rejected(self):
        self.raw.write_text('forged result',encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'hash'):self.check()
    def test_false_success_receipt_is_rejected(self):
        self.receipt['status']='failed';self.saveReceipt()
        with self.assertRaisesRegex(ValueError,'status'):self.check()
    def test_unresolvable_phase_commit_is_rejected(self):
        self.manifest['phases'][0]['verification']['commit']='0'*40
        with self.assertRaisesRegex(ValueError,'commit'):self.check()
    def test_implementation_must_exist_at_its_phase_commit(self):
        self.receipt['implementationInputs']['Project/Engine/src/Contract.hpp']='0'*64;self.saveReceipt()
        with self.assertRaisesRegex(ValueError,'[Hh]istorical'):self.check()
    def test_removed_discovered_test_is_rejected(self):
        self.discovery['tests']=[]
        with self.assertRaisesRegex(ValueError,'discovered'):self.check()
    def test_interface_symbols_must_exist(self):
        self.manifest['items'][0]['implementation']['interfaces']=['InventedContract']
        with self.assertRaisesRegex(ValueError,'interface'):self.check()
    def test_archive_path_escape_is_rejected(self):
        self.receipt['files']={'../outside.log':'0'*64};self.saveReceipt()
        with self.assertRaisesRegex(ValueError,'path'):self.check()
    def test_risk_requires_test_or_explicit_limit(self):
        self.manifest['items'][0]['implementation']['riskResolution']={}
        with self.assertRaisesRegex(ValueError,'risk'):self.check()
    def mixed_fixture(self):
        name='Project/Engine/src/Contract.hpp';raw=b'struct Contract {};\n// public boundary\r\n'
        (self.source/'src/Contract.hpp').write_bytes(raw)
        self.receipt['implementationInputs'][name]=hashlib.sha256(raw).hexdigest();self.saveReceipt()
        self.git('add','.');self.git('commit','--amend','--no-edit','-q');commit=self.git('rev-parse','HEAD').strip()
        self.manifest['phases'][0]['verification']['commit']=commit
        for item in self.manifest['items']:item['implementation']['verification']['commit']=commit
        self.manifest['historicalByteForms']={'F4':{name:{'gitCommit':commit,'rawSha256':hashlib.sha256(raw).hexdigest(),
            'normalizedSha256':hashlib.sha256(raw.replace(b'\r\n',b'\n')).hexdigest(),'crlfLineRanges':[[1,2]],'lineCount':2}}}
    def test_exact_mixed_line_ending_recipe_preserves_historical_hash(self):
        self.mixed_fixture();self.assertEqual(self.check()['coveredItems'],18)
    def test_forged_mixed_line_ending_recipe_cannot_hide_content_drift(self):
        self.mixed_fixture()
        self.manifest['historicalByteForms']['F4']['Project/Engine/src/Contract.hpp']['crlfLineRanges']=[[0,2]]
        with self.assertRaisesRegex(ValueError,'[Hh]istorical'):self.check()
if __name__=='__main__':unittest.main()
