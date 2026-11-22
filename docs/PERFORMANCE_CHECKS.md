# Performance measurements

Run **Performance measurements** manually in GitHub Actions. While working on a
branch whose workflow has not reached the default branch, use the existing entry
point:

```sh
gh workflow run android.yml --ref audit-problems -f performance_only=true
```

This runs only performance checks; it does not publish APKs or run the regression
package. Normal APK and regression dispatches keep their existing behavior.

## What is measured

Six Macrobenchmark scenarios run three times each on an API 36 x86_64 Pixel 2
emulator. The dedicated workflow accepts 1–10 iterations. The app uses a
non-debuggable, profileable build with release settings, debug signing, and full
AOT compilation. Its separate package is `com.cardputer.cardulator.benchmark`.
The harness requires an explicitly disposable emulator before clearing that
package; it never clears `com.cardputer.cardulator`.

- `coldStartup`: process-cold start, first frame, and fully drawn UI. This is not
  a first-install start or a cold filesystem cache. Fully drawn is reported when
  initialized controls are ready to draw; asynchronous restored output may follow.
- `historyScroll`: six swipes through 60 actual REPL results; setup is not timed.
- `historyPrecision`: change precision from 12 to 0 and back to 12 in that history,
  waiting for the formatted result each time. Includes dropdown interaction.
- `largeScriptTyping`: enter and delete six characters in a 500-line script;
  initial loading and opening/closing the editor are outside the measurement.
- `heavyPlot`: compute a 10,001-point sine plot and pan it six times.
- `largeArrayCalculation`: calculate `sum(sqrt((1:1:20000)))`, including persistence
  and visible result delivery. This measures completion, not permission to use
  engine controls concurrently with a running command.

Traces include app sections: `Droidulator.highlight` for editor coloring,
`Droidulator.replRebuild` for worker-side history formatting/rendering, and
`Droidulator.command` for worker evaluation plus persistence. Frame durations and
frame overruns describe rendered frames, not end-to-end key latency. Macrobenchmark
scenario durations include UI Automator calls and waits. Android JSON reports
contain repeat values, medians, and frame percentiles.

## Read the results

Open the run's **Summary** for measurements. Download its `performance-api36-*`
artifact for `summary.md`, raw `*-benchmarkData.json`, per-iteration
`.perfetto-trace` files, instrumentation reports, Gradle output and logcat. The
runner captures memory/frame snapshots and a failure screenshot/window hierarchy
before the emulator shuts down. Snapshots describe the final process state, not
per-scenario peak memory or memory leaks.

Open a trace in [Perfetto](https://ui.perfetto.dev/) or Android Studio to inspect
the main thread, rendering and worker sections. No extra tracing service or account
is required. Artifacts are retained for 30 days.

The job fails on workload errors, timeouts, missing scenarios, empty metrics or
missing iteration traces. It has no absolute millisecond limits: shared GitHub
host load makes emulator timings noisy. No automatic historical comparison or
performance-regression verdict is implemented. Retain a baseline report and
compare repeated runs under the same build, API, compilation and workload settings.
Use a physical device for reliable user-facing performance claims; the user's
phone smoke test remains separate.

## Disposable local emulator

With a running API 36 emulator and Android SDK/JDK 17 available:

```sh
PERFORMANCE_ITERATIONS=3 bash tools/run_android_performance.sh
python3 tools/summarize_performance.py macrobenchmark/build/outputs performance-artifacts
```

The benchmark module/build type is enabled only by `-PperformanceBenchmarks=true`;
normal builds do not resolve benchmark or profile-installer dependencies.

References: [Macrobenchmark setup](https://developer.android.com/topic/performance/benchmarking/macrobenchmark-overview),
[Benchmark in CI](https://developer.android.com/topic/performance/benchmarking/benchmarking-in-ci).
