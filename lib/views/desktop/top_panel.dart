import 'dart:async';

import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

import '../../l10n/app_localizations.dart';
import '../../services/battery.dart';
import '../../utils/clock.dart';
import 'quick_settings.dart';

/// The shell's top panel: app menu on the left, clock in the centre, battery
/// and quick settings on the right.
class TopPanel extends StatelessWidget {
  final BatteryStatus? battery;
  final ValueChanged<SessionAction> onSessionAction;

  const TopPanel({
    super.key,
    required this.battery,
    required this.onSessionAction,
  });

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;

    return Material(
      color: scheme.surface.withAlpha(210),
      child: SizedBox(
        height: 40,
        child: Row(
          children: [
            const SizedBox(width: 12),
            Text(
              l.appTitle,
              style: Theme.of(context)
                  .textTheme
                  .titleSmall
                  ?.copyWith(fontWeight: FontWeight.w700),
            ),
            const Expanded(child: Center(child: _Clock())),
            if (battery != null) ...[
              Tooltip(
                message: l.battery,
                child: Icon(
                  battery!.charging
                      ? Icons.battery_charging_full
                      : Icons.battery_std,
                  size: 18,
                ),
              ),
              const SizedBox(width: 4),
              Text('${battery!.capacity}%'),
              const SizedBox(width: 8),
            ],
            IconButton(
              key: const Key('quick_settings_button'),
              tooltip: l.quickSettings,
              icon: const Icon(Icons.tune, size: 18),
              onPressed: () => showQuickSettings(context, onSessionAction),
              visualDensity: VisualDensity.compact,
            ),
            const SizedBox(width: 8),
          ],
        ),
      ),
    );
  }
}

class _Clock extends StatefulWidget {
  const _Clock();

  @override
  State<_Clock> createState() => _ClockState();
}

class _ClockState extends State<_Clock> {
  late Timer _timer;
  DateTime _now = clockNow();

  @override
  void initState() {
    super.initState();
    _timer = Timer.periodic(const Duration(seconds: 15), (_) {
      setState(() => _now = clockNow());
    });
  }

  @override
  void dispose() {
    _timer.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final locale = Localizations.localeOf(context).toString();
    final time = DateFormat.Hm(locale).format(_now);
    final date = DateFormat.yMMMd(locale).format(_now);
    return Text('$date  $time', key: const Key('panel_clock'));
  }
}
