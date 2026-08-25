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

rg -q 'SceneRendererRegistry' docs/architecture.md
rg -q 'vkAcquireNextImageKHR' docs/architecture.md
rg -q 'vkQueueSubmit' docs/architecture.md
rg -q 'Face SDF' docs/character-rendering.md
rg -q 'Schwarzschild' docs/blackhole-rendering.md
rg -q 'assets_private/' docs/assets-and-editor.md
rg -q '直到项目所有者主动恢复' docs/development-and-release.md

if rg -n '\\\[|\\\]|\\\(|\\\)' docs/*.md; then
    echo 'Non-portable LaTeX delimiter found; use $ or $$ for GitHub and MkDocs' >&2
    exit 1
fi

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
