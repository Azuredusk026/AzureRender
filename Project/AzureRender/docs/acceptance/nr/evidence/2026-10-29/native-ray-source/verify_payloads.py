"""Verify archived output identities; kernel equivalence remains a separate gate."""
import gzip
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parent
summary=json.loads((root/'summary.json').read_text(encoding='utf-8'))
for record in summary['payloads']:
    resource=record['resource']
    reference=gzip.decompress((root/f'reference-{resource}.bin.gz').read_bytes())
    assert len(reference)==record['bytes']
    assert hashlib.sha256(reference).hexdigest()==record['referenceSha256']
    candidate=bytearray(reference)
    patch=json.loads(gzip.decompress((root/f'candidate-{resource}.patch.json.gz').read_bytes()))
    assert len(patch)==record['differentBytes']
    for offset,value in patch:
        assert 0<=offset<len(candidate) and 0<=value<=255
        candidate[offset]=value
    assert hashlib.sha256(candidate).hexdigest()==record['candidateSha256']
print(json.dumps(dict(status='payload-identities-verified',outputs=len(summary['payloads']),
    kernelByteEquivalence=summary['colorByteExact'] and summary['footprintByteExact'] and summary['confidenceByteExact'])))
