import hashlib,json
from pathlib import Path
run=Path(__file__).resolve().parent;path=run/'native-effects-summary.json';raw=path.read_bytes();source=json.loads(raw);assert source['status']=='measured'
expected={'hair-crown','hair-fringe','face-cheek','face-chin','eye-left'}
actors=[row for row in source['comparisons'] if row['region'] in expected]
controls=[row for row in source['comparisons'] if row['region'].startswith('environment-')]
assert len(actors)==20 and len(controls)==8 and len(source['comparisons'])==28
for effect in ['hair-kk','face-sdf','shadow','toon']:assert {row['region'] for row in actors if row['effect']==effect}==expected
summary=[]
for effect in ['hair-kk','face-sdf','shadow','toon']:
 rows=[row for row in actors if row['effect']==effect]
 summary.append(dict(effect=effect,referenceMaximumResponse=max(row['referenceMaximumResponse'] for row in rows),candidateMaximumResponse=max(row['candidateMaximumResponse'] for row in rows),maximumResponseError=max(row['maximumResponseError'] for row in rows),nonzeroReferenceRegions=[row['region'] for row in rows if row['referenceMaximumResponse']>0],passed=all(row['maximumResponseError']<=.008 for row in rows)))
report=dict(status='measured',scope='Four effects at the five frozen character interior regions. The two environment rectangles are diagnostic controls with intentionally different display presentation and do not enter the character response criterion.',scopeSource='docs/acceptance/nr/material-display-transform.md and the frozen compare_native_final.py character-only criterion',sourceReportSha256=hashlib.sha256(raw).hexdigest(),linearResponseErrorBudget=.008,characterResponseBudgetPassed=all(row['passed'] for row in summary),effects=summary,characterComparisons=actors,environmentControlDiagnostics=[{k:v for k,v in row.items() if k!='passed'} for row in controls],visualAcceptance='failed-user-review')
(run/'native-character-effects.json').write_bytes((json.dumps(report,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in report.items() if k not in {'characterComparisons','environmentControlDiagnostics'}}))
