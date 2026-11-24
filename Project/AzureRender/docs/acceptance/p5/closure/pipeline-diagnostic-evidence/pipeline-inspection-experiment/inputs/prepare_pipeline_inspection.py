from pathlib import Path

root = Path(__file__).parent
text = (root/'cache_control_experiment.py').read_text(encoding='utf-8')
text = text.replace("out = work/'cache-control-experiment'", "out = work/'pipeline-inspection-experiment'")
old = "parameter = ('disabled:' if mode == 'disabled' else '') + str(cache.resolve())"
assert text.count(old) == 1
text = text.replace(old, "parameter = 'inspect:' + str(cache.resolve())")
old = "for case in ('disabled-1', 'disabled-2', 'disabled-3', 'cold', 'warm-1', 'warm-2', 'warm-3'):"
assert text.count(old) == 1
text = text.replace(old, "for case in ('inspect-1', 'inspect-2', 'inspect-3'):")
anchor = "assert counts['graphicsCalls'] > 0 and counts['computeCalls'] > 0 and counts['failedCaches'] == 0"
assert text.count(anchor) == 1
text = text.replace(anchor, anchor+"\n        inspection = re.search(r'AZURE_DIAGNOSTIC_INSPECTION supported=(\\d+) queried=(\\d+) errors=(\\d+)', text)\n        assert inspection, 'Inspection diagnostics missing'\n        counts.update(zip(('inspectionSupported', 'inspectedPipelines', 'inspectionErrors'), map(int, inspection.groups())))\n        assert counts['inspectionSupported'] == 1 and counts['inspectionErrors'] == 0\n        assert counts['inspectedPipelines'] == counts['graphicsCalls']+counts['computeCalls']")
text = text.replace('Compare an explicit Vulkan cache', 'Query actual pipeline executable properties')
(root/'pipeline_inspection_experiment.py').write_text(text, encoding='utf-8')
print('Inspection variant prepared from the owned-child measurement harness')
