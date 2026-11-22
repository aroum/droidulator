#!/usr/bin/env bash
set -euo pipefail

PERFORMANCE_ITERATIONS="${PERFORMANCE_ITERATIONS:-3}"
if [[ ! "$PERFORMANCE_ITERATIONS" =~ ^([1-9]|10)$ ]]; then
  echo 'PERFORMANCE_ITERATIONS must be an integer from 1 to 10' >&2
  exit 1
fi

mkdir -p performance-artifacts
collect() {
  local result=$?
  trap - EXIT
  adb logcat -d > performance-artifacts/logcat.txt 2>&1 || true
  adb shell dumpsys meminfo com.cardputer.cardulator.benchmark > performance-artifacts/memory.txt 2>&1 || true
  adb shell dumpsys gfxinfo com.cardputer.cardulator.benchmark > performance-artifacts/frames.txt 2>&1 || true
  if [[ "$result" != 0 ]]; then
    adb exec-out screencap -p > performance-artifacts/failure.png 2>/dev/null || true
    adb shell uiautomator dump /sdcard/performance-window.xml >/dev/null 2>&1 || true
    adb pull /sdcard/performance-window.xml performance-artifacts/window.xml >/dev/null 2>&1 || true
  fi
  exit "$result"
}
trap collect EXIT

# Check incremental/full color equivalence on the same isolated benchmark package
# before measuring it. The class filter belongs only to this invocation.
./gradlew :app:testBenchmarkUnitTest :app:connectedBenchmarkAndroidTest \
  -PperformanceBenchmarks=true -PtestAbi=x86_64 \
  -Pandroid.testInstrumentationRunnerArguments.ephemeralDevice=true \
  '-Pandroid.testInstrumentationRunnerArguments.class=com.cardputer.cardulator.AndroidRegressionTest#highlightingIsImmediateAndCommentsMaskTokensInBothEditors' \
  --max-workers=2 --stacktrace 2>&1 | tee performance-artifacts/highlighting-regression.log

python3 - <<'PY'
from pathlib import Path
import xml.etree.ElementTree as ET
cases = [case for report in Path('app/build/outputs/androidTest-results/connected/benchmark').rglob('TEST-*.xml')
         for case in ET.parse(report).getroot().iter('testcase')
         if case.get('name') == 'highlightingIsImmediateAndCommentsMaskTokensInBothEditors']
assert len(cases) == 1, f'Expected one highlighting regression result, found {len(cases)}'
assert not any(cases[0].find(tag) is not None for tag in ('failure', 'error', 'skipped'))
PY

# Suppress only the expected emulator warning. Debuggable/not-profileable errors remain fatal.
./gradlew :macrobenchmark:connectedBenchmarkAndroidTest \
  -PperformanceBenchmarks=true -PtestAbi=x86_64 \
  -Pandroid.testInstrumentationRunnerArguments.ephemeralDevice=true \
  -Pandroid.testInstrumentationRunnerArguments.performanceIterations="$PERFORMANCE_ITERATIONS" \
  -Pandroid.testInstrumentationRunnerArguments.androidx.benchmark.suppressErrors=EMULATOR \
  --max-workers=2 --stacktrace 2>&1 | tee performance-artifacts/gradle.log
