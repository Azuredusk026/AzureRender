from pathlib import Path
run=Path(__file__).resolve().parent
for name in ['source-environment-control-source-shadow-off','control-capture-summary-source-shadow-off.json','control-source-shadow-off-driver.log']:
 p=run/name;q=run/('failed-effect-enum-'+name);assert p.resolve().parent==run.resolve() and q.resolve().parent==run.resolve() and not q.exists();p.rename(q)
print('Preserved rejected diagnostic invocation')
