"""Rebuild the editor integration proof from Git and retained evidence."""
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import tempfile
import zipfile

OUTPUT = Path(__file__).resolve().parent
REPO = OUTPUT.parents[4]
MAINTENANCE = Path("D:/Projects/FYP/Project/AzureRender/build/maintenance")
BASE = "dbe4ab3f84f55ff6776634842c4ac794cf887345"
EDITOR = "5d4cf7dbf92b9a05181a7e11a2ff81b4cca7f3b3"
TREE = "271036aaffed05620a57eebf11cd25295d5385a9"
INDEX = "Project/AzureRender/docs/plans/editor-interaction-evidence.json"
RUNS = [
    ("outlinerSearchRestore", "ca88d062b6da2b50", 1475),
    ("nodeReferenceReveal", "429b66b2d34e803e", 1497),
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git(*args, env=None):
    return subprocess.check_output(["git", "-c", "core.longpaths=true", *args],
                                   cwd=REPO, env=env)


def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def write_json(path, data):
    save(path, (json.dumps(data, ensure_ascii=False, indent=2) + "\n").encode())


def prove_patch(path, base, expected_tree):
    with tempfile.TemporaryDirectory(prefix="git-index-", dir=OUTPUT) as temporary:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(temporary) / "index"))
        git("read-tree", base, env=env)
        git("apply", "--cached", "--check", str(path), env=env)
        git("apply", "--cached", str(path), env=env)
        assert git("write-tree", env=env).decode().strip() == expected_tree


def main():
    index_bytes = git("show", EDITOR + ":" + INDEX)
    index = json.loads(index_bytes)
    assert git("merge-tree", "--write-tree", BASE, EDITOR).decode().strip() == TREE
    common = git("merge-base", BASE, EDITOR).decode().strip()
    changed = lambda a, b: set(git("diff", "--name-only", a, b).decode().splitlines())
    cross = sorted(changed(common, BASE) & changed(common, EDITOR))
    delta = sorted(changed(EDITOR, TREE))
    assert delta == ["Project/AzureRender/docs/acceptance/nr/joint-nonlocal-texture-placement.md"]
    save(OUTPUT / "source-index.json", index_bytes)
    with sqlite3.connect((MAINTENANCE / "artifact-index.sqlite3").as_uri() + "?mode=ro",
                         uri=True) as database:
        records = {name: json.loads(data) for name, data in database.execute(
            "SELECT name,data FROM runs WHERE name IN (?,?)",
            tuple(Path(index[key]["run"]).name for key, _, _ in RUNS))}
    evidence = []
    for key, suffix, dispatch in RUNS:
        item = index[key]
        name = Path(item["run"]).name
        record = records[name]
        manifest_path = MAINTENANCE / "sealed" / (name + "-" + suffix + ".json")
        archive_path = MAINTENANCE / "evidence" / (name + "-" + suffix + ".zip")
        manifest_bytes = manifest_path.read_bytes()
        assert sha(manifest_bytes) == record["manifestSha256"]
        assert sha(archive_path.read_bytes()) == record["evidenceSha256"]
        manifest = json.loads(manifest_bytes)
        retained, omitted, mismatched = [], [], []
        local = OUTPUT / ("dispatch-" + str(dispatch))
        selected = ["verify.cmd", "process.log", "release-ctest.log", "release-build.log",
                    item["selectionSummary"]]
        selected.append(item.get("creationSummary", item.get("propertiesSummary")))
        if dispatch == 1497:
            selected.extend(["integration-receipt.json", "outliner-integration.json",
                "editor-5d4cf7d-on-dbe4ab3.patch", "editor-e729305-on-783889f.patch"])
        with zipfile.ZipFile(archive_path) as archive:
            entries = set(archive.namelist())
            for path, expected in manifest["files"].items():
                member = "files/" + path
                if member not in entries:
                    omitted.append(path)
                elif sha(archive.read(member)) != expected["sha256"]:
                    mismatched.append(path)
                else:
                    retained.append(path)
            assert not mismatched, mismatched
            for path in selected:
                assert path in retained, path
                save(local / path, archive.read("files/" + path))
            for workflow in ("exploration", "inspector"):
                if dispatch == 1475:
                    trace = str(Path(item["selectionSummary"]).parent / workflow /
                                "reveal-search/evidence.json").replace("\\", "/")
                else:
                    trace = "properties/locate-node-" + workflow + "/evidence.json"
                assert trace in retained
                events = json.loads(archive.read("files/" + trace))
                failed = [event for event in events if
                          event.get("response", {}).get("passed") is False]
                assert not failed, (trace, failed)
            cpu = archive.read("files/release-ctest.log").decode()
            assert f"0 tests failed out of {item['cpuCasesPassed']}" in cpu
            summary = json.loads(archive.read("files/" + item["selectionSummary"]))
            assert len(summary) == 2 and all(case["passed"] for case in summary)
            if dispatch == 1475:
                creation = json.loads(archive.read("files/" + item["creationSummary"]))
                assert creation["passed"] and len(creation["cases"]) == 4
            else:
                properties = json.loads(archive.read("files/" + item["propertiesSummary"]))
                assert properties["passed"] and properties["nodeReferenceLocatesInBothProjects"]
        process_path = MAINTENANCE / "process-receipts" / (name + ".json")
        process_bytes = process_path.read_bytes()
        process = json.loads(process_bytes)
        assert process["exitCode"] == 0 and process["status"] == "exited"
        save(local / "managed-process-receipt.json", process_bytes)
        evidence.append(dict(dispatch=dispatch, run=name, recordStatus=record["status"],
            manifest=str(manifest_path), manifestSha256=sha(manifest_bytes),
            archive=str(archive_path), archiveSha256=record["evidenceSha256"],
            processReceipt=str(process_path), processReceiptSha256=sha(process_bytes),
            localCopies={path: manifest["files"][path]["sha256"] for path in selected},
            retainedVerifiedFiles=len(retained), omittedFiles=omitted,
            executableSha256FromCommittedIndex=item["candidateExecutableSha256"],
            cpuCases=item["cpuCasesPassed"], twoProjectSelectionSummary=summary))
    historical = OUTPUT / "dispatch-1497/editor-5d4cf7d-on-dbe4ab3.patch"
    assert sha(historical.read_bytes()) == "eff0b3168fb28177fb5451c24bc25911e863993fe57d0cfc86311d648b4f7770"
    prove_patch(historical, BASE, TREE)
    first_patch = OUTPUT / "dispatch-1497/editor-e729305-on-783889f.patch"
    first_tree = git("merge-tree", "--write-tree", "783889f", "e729305").decode().strip()
    prove_patch(first_patch, "783889f", first_tree)
    queue_path = Path("D:/Projects/FYP-coordination/resources/dispatch.sqlite3")
    with sqlite3.connect(queue_path.as_uri() + "?mode=ro", uri=True) as database:
        queue = database.execute(
            "SELECT id,task,state,exit_code FROM jobs WHERE id IN (1475,1497)").fetchall()
    receipt = dict(date="2026-11-19", base=BASE, editor=EDITOR, targetTree=TREE,
        firstStage=dict(commit=git("rev-parse", "e729305").decode().strip(), tree=first_tree),
        sourceIndex=dict(commit=EDITOR, path=INDEX, sha256=sha(index_bytes)),
        patch=str(historical.relative_to(OUTPUT)).replace("\\", "/"),
        patchSha256=sha(historical.read_bytes()), isolatedIndexVerified=True,
        committedCrossFiles=cross, differencesFromTestedTree=delta, evidence=evidence,
        currentQueue=dict(path=str(queue_path), rows=queue),
        scope="历史 Release 验收证据与接入树复算",
        pending=["主线实际合入及集成负责人确认必要复测范围", "实体键鼠、输入法和多屏现场复核"],
        limitations=["活动队列已无 1475、1497 的原始行，编号关联来自提交内索引",
                    "程序文件与完整运行目录已回收，程序摘要来自提交内索引",
                    "本次执行证据哈希、归档日志与 Git 树核验，功能结果引用历史回放"])
    write_json(OUTPUT / "integration-receipt.json", receipt)
    print(json.dumps(dict(targetTree=TREE, crossFiles=cross,
        receiptSha256=sha((OUTPUT / "integration-receipt.json").read_bytes()),
        archivesVerified=len(evidence), bytes=sum(p.stat().st_size for p in OUTPUT.rglob("*")
                                                if p.is_file())), ensure_ascii=False))


if __name__ == "__main__":
    main()
