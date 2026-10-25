import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../l10n/app_localizations.dart';
import '../../services/app_launcher.dart';
import '../../services/platform_services.dart';
import '../../services/battery.dart';
import '../../services/session_actions.dart';
import '../../state/app_state.dart';
import '../../widgets/wallpaper.dart';
import 'quick_settings.dart';
import 'top_panel.dart';

/// The shell desktop: painted wallpaper, the top panel, and session actions
/// wired to loginctl/systemctl through an injectable runner.
class DesktopPage extends StatefulWidget {
  final SysfsBatteryReader reader;
  final SessionActions session;

  const DesktopPage({
    super.key,
    required this.reader,
    required this.session,
  });

  @override
  State<DesktopPage> createState() => _DesktopPageState();
}

class _DesktopPageState extends State<DesktopPage> {
  BatteryStatus? _battery;

  @override
  void initState() {
    super.initState();
    widget.reader.read().then((status) {
      if (mounted) setState(() => _battery = status);
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
