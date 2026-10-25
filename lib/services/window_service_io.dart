// coverage:ignore-file
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:window_manager/window_manager.dart';

/// Sizes and centres the desktop window before showing it.
Future<void> initializeWindow() async {
  if (!Platform.isLinux && !Platform.isMacOS && !Platform.isWindows) {
    return;
  }

  await windowManager.ensureInitialized();

  const windowOptions = WindowOptions(
    size: Size(1280, 720),
    minimumSize: Size(560, 400),
    center: true,
    title: 'Oasis',
    backgroundColor: Colors.transparent,
    skipTaskbar: false,
    titleBarStyle: TitleBarStyle.normal,
  );

  await windowManager.waitUntilReadyToShow(windowOptions, () async {
    await windowManager.show();
    await windowManager.focus();
  });
}
