// coverage:ignore-file
import 'package:window_manager/window_manager.dart' as wm;

import 'window_actions_stub.dart' show WindowActions;

export 'window_actions_stub.dart' show WindowActions;

/// Window actions backed by the real desktop window.
class WindowActionsImpl implements WindowActions {
  const WindowActionsImpl();

  @override
  Future<void> close() => wm.windowManager.close();

  @override
  Future<void> minimize() => wm.windowManager.minimize();

  @override
  Future<void> toggleMaximize() async {
    if (await wm.windowManager.isMaximized()) {
      await wm.windowManager.unmaximize();
    } else {
      await wm.windowManager.maximize();
    }
  }

  @override
  Future<void> startDragging() => wm.windowManager.startDragging();

  @override
  Future<bool> isMaximized() => wm.windowManager.isMaximized();
}

WindowActions defaultWindowActions() => const WindowActionsImpl();
