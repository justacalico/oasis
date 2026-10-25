import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/app_localizations.dart';
import '../services/app_launcher.dart';
import '../services/platform_services.dart';
import '../services/session_actions.dart';
import '../state/app_state.dart';
import 'about/about_page.dart';
import 'apps/apps_page.dart';
import 'desktop/desktop_page.dart';
import 'settings/settings_page.dart';
import 'sidebar.dart';

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

/// The shared shell: wide row-list sidebar and the destination pages. On
/// narrow layouts the sidebar becomes a drawer but stays a row list.
class OasisShell extends StatelessWidget {
  final ShellServices services;

  const OasisShell({super.key, required this.services});

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final wide = constraints.maxWidth >= 720;
        return wide
            ? _WideShell(services: services)
            : _NarrowShell(services: services);
      },
    );
  }
}

class _WideShell extends StatelessWidget {
  final ShellServices services;

  const _WideShell({required this.services});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Row(
        children: [
          const OasisSidebar(),
          const VerticalDivider(width: 1),
          Expanded(child: _DestinationView(services: services)),
        ],
      ),
    );
  }
}

class _NarrowShell extends StatelessWidget {
  final ShellServices services;

  const _NarrowShell({required this.services});

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    return Scaffold(
      drawer: Builder(
        builder: (context) => Drawer(
          width: 240,
          child: OasisSidebar(
            onNavigate: () => Scaffold.maybeOf(context)?.closeDrawer(),
          ),
        ),
      ),
      body: Column(
        children: [
          SafeArea(
            child: Row(
              children: [
                Builder(
                  builder: (context) => IconButton(
                    key: const Key('drawer_button'),
                    icon: const Icon(Icons.menu),
                    onPressed: () => Scaffold.of(context).openDrawer(),
                  ),
                ),
                Text(
                  l.appTitle,
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ],
            ),
          ),
          Expanded(child: _DestinationView(services: services)),
        ],
      ),
    );
  }
}

class _DestinationView extends StatelessWidget {
  final ShellServices services;

  const _DestinationView({required this.services});

  @override
  Widget build(BuildContext context) {
    final index =
        context.select<OasisState, int>((s) => s.destinationIndex);
    return switch (AppDestination.values[index]) {
      AppDestination.desktop => DesktopPage(
          reader: services.batteryReader,
          session: services.sessionActions,
          loader: services.entriesLoader,
          launcher: services.appLauncher,
        ),
      AppDestination.apps => AppsPage(
          loader: services.entriesLoader,
          launcher: services.appLauncher,
        ),
      AppDestination.settings => const SettingsPage(),
      AppDestination.about => const AboutPage(),
    };
  }
}
