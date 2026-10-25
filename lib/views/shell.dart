import 'dart:async';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/desktop_entry.dart';
import '../services/app_launcher.dart';
import '../services/compositor_service.dart';
import '../services/platform_services.dart';
import '../services/session_actions.dart';
import '../state/app_state.dart';
import 'about/about_page.dart';
import 'apps/apps_overlay.dart';
import 'desktop/desktop_page.dart';
import 'settings/settings_page.dart';

/// Every service the shell needs, bundled so tests can inject fakes.
class ShellServices {
  final DesktopEntriesLoader entriesLoader;
  final AppLauncher appLauncher;
  final SysfsBatteryReader batteryReader;
  final SessionActions sessionActions;
  final CompositorService windows;

  const ShellServices({
    required this.entriesLoader,
    required this.appLauncher,
    required this.batteryReader,
    required this.sessionActions,
    required this.windows,
  });

  /// Wires up the real platform services. The entries scan is kicked off
  /// here so the applications overlay opens instantly.
  factory ShellServices.platform() {
    final run = processRunner();
    final entriesLoader = defaultEntriesLoader();
    unawaited(entriesLoader.load().catchError((_) => const <DesktopEntry>[]));
    final windows = CompositorService();
    unawaited(windows.init());
    return ShellServices(
      entriesLoader: entriesLoader,
      appLauncher: AppLauncher(run),
      batteryReader: defaultBatteryReader(),
      sessionActions: SessionActions(run),
      windows: windows,
    );
  }
}

/// The shared shell: a single full-screen surface showing the current
/// destination page, with the applications grid layered on top as a
/// Launchpad-style overlay.
class OasisShell extends StatelessWidget {
  final ShellServices services;

  const OasisShell({super.key, required this.services});

  @override
  Widget build(BuildContext context) {
    final index =
        context.select<OasisState, int>((s) => s.destinationIndex);
    final launcherOpen =
        context.select<OasisState, bool>((s) => s.launcherOpen);
    return Scaffold(
      body: Stack(
        fit: StackFit.expand,
        children: [
          switch (AppDestination.values[index]) {
            AppDestination.desktop => DesktopPage(
                reader: services.batteryReader,
                session: services.sessionActions,
                launcher: services.appLauncher,
                windows: services.windows,
              ),
            AppDestination.settings => const SettingsPage(),
            AppDestination.about => const AboutPage(),
          },
          if (launcherOpen)
            AppsOverlay(
              loader: services.entriesLoader,
              launcher: services.appLauncher,
            ),
        ],
      ),
    );
  }
}
