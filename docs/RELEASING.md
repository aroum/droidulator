# Preparing a release

1. Add reviewed English release notes without emoji at `docs/releases/<tag>.md`.
2. Merge the release changes into `main`. Include `[skip nightly]` in the merge
   commit message when publication must wait for release-note review. This keeps
   APK builds enabled but skips replacing/publishing the nightly release.
3. Create a draft GitHub release targeting the exact `main` commit, then push its
   CalVer tag (`YY.MM.DD`). The tag workflow builds signed release APKs and uploads
   them to the draft with the matching notes. Missing notes fail the release step.
4. Review the draft notes and verify the release build and APK assets before
   publishing. Tag workflows leave releases as drafts; publication is explicit.

Do not publish a draft whose build failed or whose release assets are incomplete.
