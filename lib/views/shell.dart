import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../services/app_launcher.dart';
import '../services/platform_services.dart';
import '../services/session_actions.dart';
import '../state/app_state.dart';
import 'about/about_page.dart';
import 'apps/apps_page.dart';
import 'desktop/desktop_page.dart';
import 'settings/settings_page.dart';

/// Every service the shell needs, bundled so tests can inject fakes.
class ShellServices {
  final DesktopEntriesLoader entriesLoader;
  final AppLauncher appLauncher;
  final SysfsBatteryReader batteryReader;
  final SessionActions sessionActions;

  const ShellServices({
    required this.entriesLoader,
    required this.appLauncher,
    required this.batteryReader,
    required this.sessionActions,
  });

  /// Wires up the real platform services.
  factory ShellServices.platform() {
    final run = processRunner();
    return ShellServices(
      entriesLoader: defaultEntriesLoader(),
      appLauncher: AppLauncher(run),
      batteryReader: defaultBatteryReader(),
      sessionActions: SessionActions(run),
    );
  }
}

/// The shared shell: a single full-screen surface showing the current
/// destination page.
class OasisShell extends StatelessWidget {
  final ShellServices services;

  const OasisShell({super.key, required this.services});

  @override
  Widget build(BuildContext context) {
    final index =
        context.select<OasisState, int>((s) => s.destinationIndex);
    return Scaffold(
      body: switch (AppDestination.values[index]) {
        AppDestination.desktop => DesktopPage(
            reader: services.batteryReader,
            session: services.sessionActions,
            launcher: services.appLauncher,
          ),
        AppDestination.apps => AppsPage(
            loader: services.entriesLoader,
            launcher: services.appLauncher,
          ),
        AppDestination.settings => const SettingsPage(),
        AppDestination.about => const AboutPage(),
      },
    );
  }
}
