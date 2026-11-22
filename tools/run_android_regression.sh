#!/usr/bin/env bash
set -euo pipefail
output="${TEST_OUTPUT_DIR:-test-artifacts/android}"
mkdir -p "$output"
collect_diagnostics() {
  result=$?
  trap - EXIT
  adb logcat -d > "$output/logcat.txt" 2>&1 || true
  if [ "$result" -ne 0 ]; then
    adb exec-out screencap -p > "$output/failure-screen.png" 2>/dev/null || true
  fi
  exit "$result"
}
trap collect_diagnostics EXIT
adb logcat -c
adb shell setprop debug.checkjni 1
./gradlew testCiUnitTest connectedCiAndroidTest -PtestAbi=x86_64 \
  -Pandroid.testInstrumentationRunnerArguments.ephemeralDevice=true \
  --max-workers=2 --stacktrace
python3 - <<'PY'
from pathlib import Path
import xml.etree.ElementTree as ET
reports = list(Path('app/build/outputs/androidTest-results').rglob('TEST-*.xml'))
assert reports, 'Android instrumentation produced no test reports'
suites = [ET.parse(report).getroot() for report in reports]
tests = sum(int(s.attrib.get('tests', 0)) for s in suites)
failures = sum(int(s.attrib.get('failures', 0)) + int(s.attrib.get('errors', 0)) for s in suites)
skipped = sum(int(s.attrib.get('skipped', 0)) for s in suites)
assert tests >= 14 and failures == 0 and skipped == 0, (tests, failures, skipped)
print(f'Android regression: {tests} passed, no failures or skips')
PY
