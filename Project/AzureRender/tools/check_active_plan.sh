#!/usr/bin/env bash
set -euo pipefail

test -f docs/ACTIVE_DEVELOPMENT_PLAN_CN.md
test -f docs/README_CN.md
test -f docs/PROJECT_OVERVIEW_CN.md
test -f docs/DEVELOPMENT_GUIDE_CN.md
test -f docs/ARCHITECTURE_CN.md
test -f docs/ASSET_AND_VISUAL_QA_CN.md
test -f docs/RELEASE_AND_ACCEPTANCE_CN.md

rg -q '当前阶段：P[012] (Ready|Active|Complete)' docs/ACTIVE_DEVELOPMENT_PLAN_CN.md
rg -q '\| 1 \| P0 \| (Ready|Active|Complete) \|' docs/ACTIVE_DEVELOPMENT_PLAN_CN.md
rg -q '\| 2 \| P1 \| (Ready|Active|Complete) \|' docs/ACTIVE_DEVELOPMENT_PLAN_CN.md
rg -q '\| 3 \| P2 \| (Ready|Active|Complete) \|' docs/ACTIVE_DEVELOPMENT_PLAN_CN.md
rg -q '唯一事实来源' docs/ACTIVE_DEVELOPMENT_PLAN_CN.md

if rg -n 'AR-5\.2.*(next|下一)|v5 队列全部 Complete|CTest 11/11|当前节点：AR-' \
    README.md docs/*.md; then
    echo 'Stale active-plan text found' >&2
    exit 1
fi

if rg -n 'archive/.+.*(当前任务|唯一事实来源)' docs/*.md; then
    echo 'Archive content must not be presented as an active source' >&2
    exit 1
fi

git diff --check
