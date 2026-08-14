#!/usr/bin/env bash
set -euo pipefail

test -f docs/ACTIVE_DEVELOPMENT_PLAN_CN.md
test -f docs/PROJECT_HANDOFF_CN.md
test -f docs/RENDERER_MODULARIZATION_PLAN_CN.md
test -f docs/RC0_BASELINE_CN.md

rg -q '当前节点：AR-[45]\.[0-9] (Ready|Complete)' docs/ACTIVE_DEVELOPMENT_PLAN_CN.md
rg -q 'AR-4.5.*(Backlog|Ready|Complete)' docs/ACTIVE_DEVELOPMENT_PLAN_CN.md

if rg -n 'M3 Ready|M2 release-gate review is next|AR-3.1 可替换' \
    README.md docs/PROJECT_HANDOFF_CN.md docs/RENDERER_MODULARIZATION_PLAN_CN.md; then
    echo 'Stale active-plan text found' >&2
    exit 1
fi

git diff --check
