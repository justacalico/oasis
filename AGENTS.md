This is a Wayland desktop environment shell for Linux, written in Flutter.

- Refactor rather than duplicating code
- Goal is an A+ codebase
- Keep 100% test coverage; every line must be exercised by tests
- Goldens for every screen; regenerate with `flutter test --update-goldens`
- All user-facing strings go through the l10n ARB files (en, zh, ru, ga)
- Commits use Conventional Commits; cocogitto handles versioning
- The landing page lives in landing/ and is a separate Flutter web project
