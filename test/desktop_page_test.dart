import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/l10n/app_localizations.dart';
import 'package:oasis/services/battery.dart';
import 'package:oasis/state/app_state.dart';
import 'package:oasis/views/desktop/dock.dart';
import 'package:oasis/views/desktop/top_panel.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  testWidgets('shows the panel clock and battery percentage', (tester) async {
    await pumpShell(
      tester,
      services: fakeServices(
        battery: const BatteryStatus(capacity: 87, charging: false),
      ),
    );
    await tester.pump();
    expect(find.byKey(const Key('panel_clock')), findsOneWidget);
    expect(find.text('87%'), findsOneWidget);
    expect(find.byIcon(Icons.battery_std), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('charging battery uses the charging icon', (tester) async {
    await pumpShell(
      tester,
      services: fakeServices(
        battery: const BatteryStatus(capacity: 50, charging: true),
      ),
    );
    await tester.pump();
    expect(find.byIcon(Icons.battery_charging_full), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('hides the battery row without a battery', (tester) async {
    await pumpShell(tester);
    await tester.pump();
    expect(find.byIcon(Icons.battery_std), findsNothing);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('quick settings runs session actions', (tester) async {
    final runner = RecordingRunner();
    await pumpShell(tester, services: fakeServices(runner: runner));
    await tester.pump();

    await tester.tap(find.byKey(const Key('quick_settings_button')));
    await tester.pumpAndSettle();
    expect(find.text('Lock'), findsOneWidget);
    expect(find.text('Suspend'), findsOneWidget);
    expect(find.text('Restart'), findsOneWidget);
    expect(find.text('Power off'), findsOneWidget);

    await tester.tap(find.byKey(const Key('session_lock')));
    await tester.pumpAndSettle();
    expect(runner.calls, ['loginctl lock-session']);

    for (final key in [
      'session_suspend',
      'session_restart',
      'session_powerOff',
    ]) {
      await tester.tap(find.byKey(const Key('quick_settings_button')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(Key(key)));
      await tester.pumpAndSettle();
    }
    expect(runner.calls, [
      'loginctl lock-session',
      'systemctl suspend',
      'systemctl reboot',
      'systemctl poweroff',
    ]);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('dock launches entries through the launcher', (tester) async {
    final runner = RecordingRunner();
    await pumpShell(tester, services: fakeServices(runner: runner));
    tester.widget<OasisDock>(find.byType(OasisDock)).onLaunch(
      fixtureEntries.first,
    );
    await tester.pump();
    expect(runner.calls, ['gtk-launch org.test.terminal']);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('wallpaper paints for every variant', (tester) async {
    final state = await pumpShell(tester);
    for (final w in Wallpaper.values) {
      state.setWallpaper(w);
      await tester.pump();
    }
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('clock refreshes on the timer', (tester) async {
    await pumpShell(tester);
    await tester.pump(const Duration(seconds: 16));
    expect(find.byKey(const Key('panel_clock')), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('TopPanel renders without battery', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          body: TopPanel(battery: null, onSessionAction: (_) {}),
        ),
      ),
    );
    expect(find.byType(IconButton), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
