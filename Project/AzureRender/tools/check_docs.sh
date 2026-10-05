#!/usr/bin/env bash
set -euo pipefail

for document in \
    README.md \
    docs/index.md \
    docs/getting-started.md \
    docs/architecture.md \
    docs/character-rendering.md \
    docs/blackhole-rendering.md \
    docs/assets-and-editor.md \
    docs/development-and-release.md \
    docs/reference.md; do
    test -f "$document"
done

test -f mkdocs.yml
test -f requirements-docs.txt
test -f tools/check_doc_style.py

rg -q 'SceneRendererRegistry' docs/architecture.md
rg -q 'vkAcquireNextImageKHR' docs/architecture.md
rg -q 'vkQueueSubmit' docs/architecture.md
rg -q 'GpuAllocator' docs/architecture.md
rg -q 'Face SDF' docs/character-rendering.md
rg -q 'Schwarzschild' docs/blackhole-rendering.md
rg -q 'assets_private/' docs/assets-and-editor.md
rg -q 'baselines/character' docs/assets-and-editor.md
rg -q '等待项目所有者主动恢复' docs/development-and-release.md

# The product plan is the only active stage tracker.
test -f docs/plans/azure-engine-plan.md
test -f docs/runtime/rhi-synchronization.md
test -f docs/acceptance/r1/2026-09-26.md
rg -q 'Deferred' docs/plans/azure-engine-plan.md

# E0 deliverables must stay present once the stage is complete.
test -f tools/run_visual_regression.py
test -f tools/run_performance_baseline.py
test -f tools/visual_regression_cases.json
test -d assets_public/baselines/character

if rg -n '\\\[|\\\]|\\\(|\\\)' docs/*.md; then
    echo 'Non-portable LaTeX delimiter found; use $ or $$ for GitHub and MkDocs' >&2
    exit 1
fi

style_documents=(README.md docs/*.md docs/plans/*.md docs/runtime/*.md docs/tutorials/*.md)
if test -f ../../README.md; then
    style_documents+=(../../README.md)
fi

if rg -n '；|—|–' "${style_documents[@]}"; then
    echo 'Dense punctuation found in active documentation; use short sentences' >&2
    exit 1
fi

python tools/check_doc_style.py "${style_documents[@]}"

if rg -n \
    'ARCHITECTURE_CN|USER_GUIDE_CN|DEVELOPMENT_ROADMAP_CN|ACTIVE_DEVELOPMENT_PLAN_CN|PROJECT_OVERVIEW_CN' \
    README.md docs/*.md portfolio/*.md CMakeLists.txt; then
    echo 'Stale documentation entry found' >&2
    exit 1
fi

if find portfolio/images -type f | \
    rg '/(P[0-9]+|S[0-9]+|CQ[0-9]+|final_final|capture_[0-9]+)'; then
    echo 'Task-oriented screenshot name found in portfolio' >&2
    exit 1
fi

git diff --check

python tools/audit_repository.py --source ../.. --output build/repository-audit.json
