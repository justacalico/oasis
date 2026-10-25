import 'package:flutter/material.dart';

import '../../l10n/app_localizations.dart';

enum SessionAction { lock, suspend, restart, powerOff }

/// Opens the quick-settings popover below the panel.
void showQuickSettings(
  BuildContext context,
  ValueChanged<SessionAction> onAction,
) {
  final l = AppLocalizations.of(context);
  showDialog<void>(
    context: context,
    barrierColor: Colors.transparent,
    builder: (context) => QuickSettingsPanel(
      title: l.quickSettings,
      onAction: (action) {
        Navigator.of(context).pop();
        onAction(action);
      },
    ),
  );
}

class QuickSettingsPanel extends StatelessWidget {
  final String title;
  final ValueChanged<SessionAction> onAction;

  const QuickSettingsPanel({super.key, required this.title, required this.onAction});

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final actions = [
      (SessionAction.lock, Icons.lock_outline, l.sessionLock),
      (SessionAction.suspend, Icons.bedtime_outlined, l.sessionSuspend),
      (SessionAction.restart, Icons.restart_alt, l.sessionRestart),
      (SessionAction.powerOff, Icons.power_settings_new, l.sessionPowerOff),
    ];

    return Align(
      alignment: Alignment.topRight,
      child: Padding(
        padding: const EdgeInsets.only(top: 44, right: 8),
        child: Card(
          elevation: 8,
          child: Padding(
            padding: const EdgeInsets.all(12),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(title, style: Theme.of(context).textTheme.titleSmall),
                const SizedBox(height: 8),
                for (final (action, icon, label) in actions)
                  SizedBox(
                    width: 180,
                    child: TextButton.icon(
                      key: Key('session_${action.name}'),
                      icon: Icon(icon, size: 18),
                      label: Align(
                        alignment: Alignment.centerLeft,
                        child: Text(label),
                      ),
                      onPressed: () => onAction(action),
                    ),
                  ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
