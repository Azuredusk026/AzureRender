"""Evaluate unchanged archived R3 reports with the production budget checker."""
import argparse
import json
from pathlib import Path

from run_r3_performance import evaluate_budget


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    directory = args.evidence_dir
    budgets = {}
    for name, small in (("multi-resource", False), ("small-scene", True)):
        data = json.loads((directory / f"{name}-performance.json").read_text(encoding="utf-8"))
        if data["frames"] != 300 or data["repeats"] < 3 or data["instances"] != (256 if small else 32):
            raise ValueError("Archived reports do not meet R3 sampling requirements")
        budgets[name] = evaluate_budget(data["median"], small)
    result = {"standard": "r3-budget-v2", "method": "Production checker applied to unchanged archived medians",
              "budgets": budgets, "passed": all(budget["passed"] for budget in budgets.values())}
    (directory / "budget-v2-evaluation.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
