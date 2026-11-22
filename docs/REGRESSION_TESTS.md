# Regression checks

Run **Full regression checks** in GitHub Actions, or dispatch it with:

```sh
gh workflow run android.yml --ref audit-problems -f regression_only=true
```

The existing build workflow delegates to `regression.yml` when `regression_only`
is enabled. This also allows dispatching the new suite from a feature branch
before its workflow has been merged into the default branch.

The workflow runs native and checked-JNI tests, ASan/UBSan, conversion fuzzing,
Kotlin unit tests, and Android instrumentation on API 24 and API 36. Reports,
logcat and a screenshot on failure are uploaded as separate job artifacts.
It does not publish a release or require signing secrets. A negative test checks
that release validation rejects missing credentials.

The Android suite exercises the actual packaged JNI library, REPL precision,
rotation, script CRUD and drafts, Help/Units search, persisted session restoration,
corrupt storage recovery, Enter settings and plot serialization. All waits are
bounded and wait for observable state rather than fixed delays. The suite requires
at least 14 scenarios, including synchronous comment/string highlighting in both
editors, REPL input focus after scrolling results, and toolbar haptics preference
persistence across rotation. Physical vibration and vendor keyboard behavior still
need a device check.

## Disposable emulators only

Android fixtures reset `user_data.json`, plot state and application preferences.
The suite requires `ephemeralDevice=true` and checks emulator hardware before
changing data. Do not run these fixtures on a personal device.

For a disposable local emulator with the Android SDK installed:

```sh
bash tools/run_android_regression.sh
```

`-PtestAbi=x86_64` selects the CI test build and limits APK splits for the emulator.
Normal APK/release builds retain the full ABI set. The instrumentation runner and
its dependencies are confined to Android test configuration.

## Coverage limits

Recreating or finishing an Activity is not an OS process-death test. Upgrade
installation, vendor keyboards, subjective theme/layout quality, graph touch
accuracy and real-device performance remain separate checks. LeakSanitizer is
currently disabled for the GNU Units fuzz/sanitizer runs; passing them does not
prove absence of leaks. Newly added Android scenarios are unverified until their
Actions run completes successfully.

Implementation references: [AndroidJUnitRunner](https://developer.android.com/training/testing/instrumented-tests/androidx-test-libraries/runner)
and [Android Emulator Runner](https://github.com/ReactiveCircus/android-emulator-runner).
