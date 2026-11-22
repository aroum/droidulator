#!/usr/bin/env python3
"""Validate Macrobenchmark output and publish a compact GitHub Actions summary."""

import json
import math
import os
from pathlib import Path
import sys

SCENARIOS = {
    "coldStartup", "historyScroll", "historyPrecision", "largeScriptTyping",
    "heavyPlot", "largeArrayCalculation",
}


def summarize(source: Path, destination: Path) -> bool:
    destination.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Droidulator performance measurements", "",
        f"Commit: `{os.environ.get('GITHUB_SHA', 'local')}`", "",
        "API 36 emulator · x86_64 · non-debuggable benchmark build · full AOT compilation.",
        "Emulator timings include host noise and are not phone performance guarantees.",
        "No millisecond thresholds are enforced. Missing scenarios/metrics/traces and failed workloads fail the run.",
        "",
    ]
    scenarios = set()
    seen = set()
    valid = True
    expected_traces = 0
    for path in sorted(source.rglob("*-benchmarkData.json")):
        report = json.loads(path.read_text())
        for item in report.get("benchmarks", []):
            key = (item.get("className"), item.get("name"))
            if key in seen:
                continue
            seen.add(key)
            name = item["name"]
            scenario = next((s for s in SCENARIOS if name == s or name.endswith("_" + s)), None)
            if scenario:
                scenarios.add(scenario)
            repeats = item.get("repeatIterations", 0)
            valid &= isinstance(repeats, int) and 1 <= repeats <= 10
            if isinstance(repeats, int):
                expected_traces += repeats
            lines += [f"## {name}", "", f"Iterations: {item.get('repeatIterations', '?')}", ""]
            measurements = 0
            for group in ("metrics", "sampledMetrics"):
                for metric, values in sorted(item.get(group, {}).items()):
                    stats = []
                    for field in ("minimum", "median", "maximum", "P50", "P90", "P95", "P99"):
                        value = values.get(field)
                        if isinstance(value, (int, float)):
                            valid &= math.isfinite(value)
                            stats.append(f"{field}: {value:.3f}")
                    if stats:
                        measurements += 1
                        lines.append(f"- **{metric}** — " + "; ".join(stats))
            valid &= measurements > 0
            lines.append("")
    missing = SCENARIOS - scenarios
    traces = list(source.rglob("*.perfetto-trace"))
    if missing:
        lines += ["Missing scenarios: " + ", ".join(sorted(missing)), ""]
    lines += [f"Perfetto traces: {len(traces)} (expected at least {expected_traces})", "",
              "Download the performance artifact and open a `.perfetto-trace` at https://ui.perfetto.dev/ or in Android Studio.",
              "`Droidulator.highlight` measures synchronous editor coloring; `Droidulator.replRebuild` measures worker-side formatting/rendering;",
              "`Droidulator.command` measures worker evaluation plus persistence. Frame metrics measure app frames, not input-to-photon latency.",
              "UI automation and waits remain inside measured end-to-end scenarios. Memory/gfxinfo snapshots are diagnostic, not leak tests."]
    text = "\n".join(lines) + "\n"
    (destination / "summary.md").write_text(text)
    if summary := os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(summary, "a") as stream:
            stream.write(text)
    print(text)
    return valid and not missing and len(traces) >= expected_traces


if __name__ == "__main__":
    sys.exit(0 if summarize(Path(sys.argv[1]), Path(sys.argv[2])) else 1)
