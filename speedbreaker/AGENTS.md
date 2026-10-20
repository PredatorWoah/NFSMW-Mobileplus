# SpeedBreaker One engineering requirements

Before debugging, optimizing or porting, read and follow
`docs/ENGINEERING_METHODOLOGY.md`, `docs/ANDROID_PROGRESS.md` and the checkpoint
for the affected subsystem. The methodology is mandatory, including competing
hypotheses, boundary tests, evidence labels and isolated reversible changes.

Preserve Android ISO loading, audio, settings and visual diagnostics. Never commit
XEX/game assets, generated PPC code, credentials or private diagnostic logs.
Build validation must be distinguished from device validation. Preserve previous APKs.
