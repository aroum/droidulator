import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "performance_summary", Path(__file__).parents[1] / "tools/summarize_performance.py"
)
summary = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summary)


class PerformanceSummaryTest(unittest.TestCase):
    def fixture(self, root):
        report = {"benchmarks": [
            {"name": name, "className": "PerformanceBenchmark", "repeatIterations": 3,
             "metrics": {"timeMs": {"minimum": 1, "median": 2, "maximum": 3}},
             "sampledMetrics": {"frameDurationCpuMs": {"P50": 4, "P95": 8}}}
            for name in sorted(summary.SCENARIOS)
        ]}
        path = root / "app-benchmarkData.json"
        path.write_text(json.dumps(report))
        for iteration in range(18):
            (root / f"iteration{iteration}.perfetto-trace").write_bytes(b"fixture")
        return report, path

    def run_summary(self, root):
        with contextlib.redirect_stdout(io.StringIO()):
            return summary.summarize(root, root / "report")

    def test_complete_report_writes_human_summary_and_github_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            with patch.dict("os.environ", {"GITHUB_STEP_SUMMARY": str(root / "github.md")}):
                self.assertTrue(self.run_summary(root))
            text = (root / "report/summary.md").read_text()
            self.assertIn("P95: 8.000", text)
            self.assertIn("median: 2.000", text)
            self.assertIn("expected at least 18", text)
            self.assertEqual(text, (root / "github.md").read_text())

    def test_empty_or_incomplete_results_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertFalse(self.run_summary(root))
            report, path = self.fixture(root)
            report["benchmarks"].pop()
            path.write_text(json.dumps(report))
            self.assertFalse(self.run_summary(root))

    def test_missing_metrics_or_iteration_traces_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report, path = self.fixture(root)
            (root / "iteration17.perfetto-trace").unlink()
            self.assertFalse(self.run_summary(root))
            report["benchmarks"][0]["metrics"] = {}
            report["benchmarks"][0]["sampledMetrics"] = {}
            path.write_text(json.dumps(report))
            self.assertFalse(self.run_summary(root))

    def test_nonfinite_metrics_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report, path = self.fixture(root)
            report["benchmarks"][0]["metrics"]["timeMs"]["median"] = float("inf")
            path.write_text(json.dumps(report))
            self.assertFalse(self.run_summary(root))


if __name__ == "__main__":
    unittest.main()
