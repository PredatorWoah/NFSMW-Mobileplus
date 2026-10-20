# SpeedBreaker One Android project

See [build and RP6 test instructions](../scripts/android/README.md).

The Gradle wrapper is taken from SDL release-3.2.24's Android project. Gradle's
wrapper is licensed under Apache-2.0. The wrapper downloads Gradle 8.12 and
verifies its SHA-256. SDL's Java source is used from the matching pinned tools
checkout, not copied or replaced by an independent engine layer.

Diagnostic mode produces a platform probe. The default mode builds the real
SpeedBreaker runtime and requires generated PPC sources from your own XEX.
