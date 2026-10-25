/// No-op window actions used where there is no desktop window (web, tests).
class WindowActions {
  const WindowActions();

  Future<void> close() async {}
  Future<void> minimize() async {}
  Future<void> toggleMaximize() async {}
  Future<void> startDragging() async {}
  Future<bool> isMaximized() async => false;
}

WindowActions defaultWindowActions() => const WindowActions();
