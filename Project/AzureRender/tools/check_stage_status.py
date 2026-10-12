"""Validate stage completion and render the authoritative human status page."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

STATES = {'Complete': '完成', 'Ready': 'Ready', 'Active': '开发中', 'Pending': '待完成'}


def validate(data: dict, root: Path) -> list[str]:
    errors: list[str] = []
    stages = data.get('stages', [])
    ids = [s['id'] for s in stages]
    by_id = {s['id']: s for s in stages}
    if len(ids) != len(set(ids)):
        errors.append('duplicate stage id')
    if sum(s.get('status') == 'Active' for s in stages) > 1:
        errors.append('multiple active stages')
    for stage in stages:
        sid = stage['id']
        if stage.get('status') not in STATES:
            errors.append(f'{sid}: unknown stage state')
        if stage.get('status') == 'Complete' and stage.get('checksPassed') is not True:
            errors.append(f'{sid}: completion requires passed checks')
        evidence = stage.get('evidence', [])
        if stage.get('status') == 'Complete' and not evidence:
            errors.append(f'{sid}: completion requires evidence')
        for path in evidence:
            if not (root / path).is_file():
                errors.append(f'{sid}: missing evidence: {path}')
        for dep in stage.get('dependsOn', []):
            if dep not in by_id:
                errors.append(f'{sid}: unknown dependency: {dep}')
            elif stage.get('status') in ('Ready', 'Active', 'Complete'):
                if by_id[dep].get('status') != 'Complete':
                    errors.append(f'{sid}: dependency not complete: {dep}')

    visited: set[str] = set()
    visiting: set[str] = set()

    def visit(sid: str):
        if sid in visiting:
            errors.append(f'{sid}: dependency cycle')
            return
        if sid in visited or sid not in by_id:
            return
        visiting.add(sid)
        for dep in by_id[sid].get('dependsOn', []):
            visit(dep)
        visiting.remove(sid)
        visited.add(sid)

    for sid in by_id:
        visit(sid)

    gate_ids: set[str] = set()
    for gate in data.get('acceptance', []):
        gid = gate['id']
        if gid in gate_ids:
            errors.append(f'{gid}: duplicate acceptance id')
        gate_ids.add(gid)
        owner, origin = gate.get('ownerPhase'), gate.get('originPhase')
        if owner not in by_id or origin not in by_id:
            errors.append(f'{gid}: unknown acceptance owner or origin')
            continue
        if gid not in by_id[owner].get('requiredAcceptance', []):
            errors.append(f'{gid}: owner must reference owned gate')
        result = gate.get('result')
        if result not in ('passed', 'failed', 'pending', 'notExecuted'):
            errors.append(f'{gid}: unknown acceptance result')
        if result == 'passed':
            if not gate.get('evidence') or not (root / gate['evidence']).is_file():
                errors.append(f'{gid}: passed gate requires evidence')
        elif by_id[owner].get('status') == 'Complete':
            errors.append(f'{gid}: incomplete gate owned by completed stage')
        if origin != owner:
            path = gate.get('transferEvidence')
            if not path or not (root / path).is_file():
                errors.append(f'{gid}: transfer requires evidence')

    gates = {g['id']: g for g in data.get('acceptance', [])}
    for path in data.get('acceptanceSources', []):
        source = json.loads((root / path).read_text(encoding='utf-8-sig'))
        for item in source['items']:
            if item['id'] not in gates:
                errors.append(f'{path}: missing source acceptance: {item["id"]}')
    for stage in stages:
        for gid in stage.get('requiredAcceptance', []):
            if gid not in gates:
                errors.append(f'{stage["id"]}: unknown required acceptance: {gid}')
            elif stage.get('status') == 'Complete' and gates[gid]['result'] != 'passed':
                errors.append(f'{stage["id"]}: required acceptance not passed: {gid}')

    for name in data.get('mirrors', []):
        mirror = json.loads((root / name).read_text(encoding='utf-8-sig'))
        for sid, status in mirror.get('currentStageStatuses', {}).items():
            if sid not in by_id or status != by_id[sid]['status']:
                errors.append(f'{name}: mirror drift: {sid}')
        for collection in ('phases', 'stages'):
            for stage in mirror.get(collection, []):
                if isinstance(stage, dict) and stage.get('id') in by_id:
                    if stage.get('status') != by_id[stage['id']]['status']:
                        errors.append(f'{name}: mirror drift: {stage["id"]}')
    return errors


def render(data: dict, page: Path, root: Path) -> str:
    import os

    def link(path: str) -> str:
        return Path(os.path.relpath(root / path, page.parent)).as_posix()

    lines = [
        '# 当前阶段状态', '',
        '本页回答各阶段处于什么状态，下一项工作是什么。',
        '阶段状态取值为完成、Ready、开发中、待完成。',
        '完成表示本阶段规定的交付和检查已满足。',
        'Ready 表示依赖满足，可以开始执行。',
        '开发中表示当前正在执行，待完成表示尚未具备启动条件。', '',
        '机器清单为相邻的 `stage-status.json`。',
        '本页由 `tools/check_stage_status.py --write-page` 生成。', '',
        '## 阶段清单', '',
        '| 代号 | 工作 | 状态 | 范围与依据 |',
        '| --- | --- | --- | --- |',
    ]
    for s in data['stages']:
        evidence = s.get('evidence', [])
        ref = f'[记录]({link(evidence[0])})' if evidence else '规格与启动条件待落实'
        lines.append(f'| {s["id"]} | {s["name"]} | {STATES[s["status"]]} | {s.get("scope", "本阶段交付")}。{ref} |')
    lines.extend([
        '', '## 最终收尾', '',
        '已授权移交的条件由 P8 持有，原通过标准保持有效。',
        '历史失败、检查结果和候选指纹按原记录读取。',
        'P8 完成前，项目的完整交付验收仍待完成。', '',
        '| 条目 | 来源阶段 | 收尾阶段 | 当前结果 |',
        '| --- | --- | --- | --- |',
    ])
    results = {'passed': '通过', 'failed': '失败记录保留，待复核',
               'pending': '待验', 'notExecuted': '未执行'}
    for g in data.get('acceptance', []):
        lines.append(f'| {g["id"]} | {g["originPhase"]} | {g["ownerPhase"]} | {results[g["result"]]} |')
    lines.extend(['', '详细步骤见[验收收尾流程](../acceptance/phase-closure.md)。', ''])
    return '\n'.join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--manifest', default='docs/plans/stage-status.json')
    parser.add_argument('--page', default='docs/plans/current-stage-status.md')
    parser.add_argument('--write-page', action='store_true')
    args = parser.parse_args()
    try:
        data = json.loads((args.root / args.manifest).read_text(encoding='utf-8-sig'))
        errors = validate(data, args.root)
        if errors:
            print('\n'.join(errors), file=sys.stderr)
            return 1
        page = args.root / args.page
        content = render(data, page, args.root)
        if args.write_page:
            page.parent.mkdir(parents=True, exist_ok=True)
            page.write_text(content, encoding='utf-8')
        elif not page.is_file() or page.read_text(encoding='utf-8') != content:
            print('Status page is stale; run with --write-page', file=sys.stderr)
            return 1
        print(f'Stage status verified: {len(data["stages"])} stages, '
              f'{len(data.get("acceptance", []))} acceptance items.')
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f'Invalid stage status input: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
