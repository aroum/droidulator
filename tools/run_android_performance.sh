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

# Suppress only the expected emulator warning. Debuggable/not-profileable errors remain fatal.
./gradlew :macrobenchmark:connectedBenchmarkAndroidTest \
  -PperformanceBenchmarks=true -PtestAbi=x86_64 \
  -Pandroid.testInstrumentationRunnerArguments.ephemeralDevice=true \
  -Pandroid.testInstrumentationRunnerArguments.performanceIterations="$PERFORMANCE_ITERATIONS" \
  -Pandroid.testInstrumentationRunnerArguments.androidx.benchmark.suppressErrors=EMULATOR \
  --max-workers=2 --stacktrace 2>&1 | tee performance-artifacts/gradle.log
