#!/usr/bin/env bash
set -euo pipefail

for document in \
    docs/README_CN.md \
    docs/ARCHITECTURE_CN.md \
    docs/USER_GUIDE_CN.md \
    docs/DEVELOPMENT_ROADMAP_CN.md \
    docs/DEVELOPMENT_GUIDE_CN.md \
    docs/ENGINE_EVOLUTION_PLAN_CN.md \
    docs/ASSET_AND_VISUAL_QA_CN.md \
    docs/RELEASE_AND_ACCEPTANCE_CN.md; do
    test -f "$document"
done

test ! -f docs/ACTIVE_DEVELOPMENT_PLAN_CN.md
test ! -f docs/PROJECT_OVERVIEW_CN.md

rg -q 'R1：发布工程硬化' docs/DEVELOPMENT_ROADMAP_CN.md
rg -q 'E1-E7 为 `Ready` 队列，当前没有 Active 阶段' docs/DEVELOPMENT_ROADMAP_CN.md
rg -q 'E0-E7 引擎化队列' docs/DEVELOPMENT_ROADMAP_CN.md
rg -q 'E1 RHI 与内存层' docs/DEVELOPMENT_ROADMAP_CN.md
rg -q 'E7 复杂场景验证' docs/DEVELOPMENT_ROADMAP_CN.md
rg -q '无限期 Deferred' docs/DEVELOPMENT_ROADMAP_CN.md

rg -q 'E0：视觉与性能基线' docs/ENGINE_EVOLUTION_PLAN_CN.md
rg -q 'E7：复杂场景验证' docs/ENGINE_EVOLUTION_PLAN_CN.md

# E0 deliverables must stay present once the stage is complete.
test -f tools/run_visual_regression.py
test -f tools/run_performance_baseline.py
test -f tools/visual_regression_cases.json
test -d assets_public/baselines/character
rg -q 'SceneRendererRegistry' docs/ARCHITECTURE_CN.md
rg -q 'captures/<scene>/' docs/USER_GUIDE_CN.md

if rg -n 'ACTIVE_DEVELOPMENT_PLAN_CN|PROJECT_OVERVIEW_CN|check_active_plan.sh' \
    README.md docs/*.md portfolio/*.md CMakeLists.txt; then
    echo 'Stale documentation entry found' >&2
    exit 1
fi

if find portfolio/images -type f | rg '/(P[0-9]+|S[0-9]+|CQ[0-9]+|final_final|capture_[0-9]+)'; then
    echo 'Task-oriented screenshot name found in portfolio' >&2
    exit 1
fi

git diff --check
