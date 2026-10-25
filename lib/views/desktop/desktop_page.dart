import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../l10n/app_localizations.dart';
import '../../models/desktop_entry.dart';
import '../../services/app_launcher.dart';
import '../../services/platform_services.dart';
import '../../services/battery.dart';
import '../../services/session_actions.dart';
import '../../state/app_state.dart';
import '../../widgets/wallpaper.dart';
import 'dock.dart';
import 'quick_settings.dart';
import 'top_panel.dart';

/// The shell desktop: painted wallpaper, the top panel, the application dock,
/// and session actions wired to loginctl/systemctl through an injectable
/// runner.
class DesktopPage extends StatefulWidget {
  final SysfsBatteryReader reader;
  final SessionActions session;
  final DesktopEntriesLoader loader;
  final AppLauncher launcher;

  const DesktopPage({
    super.key,
    required this.reader,
    required this.session,
    required this.loader,
    required this.launcher,
  });

  @override
  State<DesktopPage> createState() => _DesktopPageState();
}

class _DesktopPageState extends State<DesktopPage> {
  BatteryStatus? _battery;
  List<DesktopEntry> _entries = const [];

  @override
  void initState() {
    super.initState();
    widget.reader.read().then((status) {
      if (mounted) setState(() => _battery = status);
    });
    widget.loader.load().then((entries) {
      if (mounted) setState(() => _entries = entries);
    });
  }

  void _onSessionAction(SessionAction action) {
    final session = widget.session;
    switch (action) {
      case SessionAction.lock:
        session.lock();
      case SessionAction.suspend:
        session.suspend();
      case SessionAction.restart:
        session.restart();
      case SessionAction.powerOff:
        session.powerOff();
    }
  }

  @override
  Widget build(BuildContext context) {
    final wallpaper =
        context.select<OasisState, Wallpaper>((s) => s.wallpaper);

    return Stack(
      fit: StackFit.expand,
      children: [
        WallpaperView(wallpaper: wallpaper),
        Align(
          alignment: Alignment.topCenter,
          child: TopPanel(
            battery: _battery,
            onSessionAction: _onSessionAction,
          ),
        ),
        Align(
          alignment: Alignment.bottomCenter,
          child: OasisDock(
            entries: _entries,
            onLaunch: (entry) =>
                launchEntry(context, widget.launcher, entry),
            onShowApps: () => context
                .read<OasisState>()
                .setDestinationIndex(AppDestination.apps.index),
          ),
        ),
      ],
    );
  }
}

/// Shows a snackbar when an application fails to launch.
void showLaunchFailed(BuildContext context, String name) {
  final l = AppLocalizations.of(context);
  ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(content: Text(l.launchFailed(name))),
  );
}

/// Launches [entry] and reports failures on the snackbar.
Future<void> launchEntry(
  BuildContext context,
  AppLauncher launcher,
  entry,
) async {
  final code = await launcher.launch(entry);
  if (code != 0 && context.mounted) {
    showLaunchFailed(context, entry.name);
  }
}
