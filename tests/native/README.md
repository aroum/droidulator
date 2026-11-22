# Native checks

From the repository root, run the engine tests without the Android SDK:

```sh
cmake -S tests/native -B /tmp/droidulator-native -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/droidulator-native --parallel
ctest --test-dir /tmp/droidulator-native --output-on-failure
```

The optional Clang/libFuzzer target is run in CI with ASan and UBSan. Leak detection is disabled there because the incompatible-units leak is tracked separately. Release builds require an increasing `VERSION_CODE` (or `-PversionCode`) and the `RELEASE_KEYSTORE_PATH`, `RELEASE_KEYSTORE_PASSWORD`, `RELEASE_KEY_ALIAS`, and `RELEASE_KEY_PASSWORD` environment variables. GitHub Actions uses its workflow run number as the version code.
