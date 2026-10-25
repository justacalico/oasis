import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/views/about/about_page.dart';
import 'package:oasis/views/apps/apps_page.dart';
import 'package:oasis/views/desktop/desktop_page.dart';
import 'package:oasis/views/settings/settings_page.dart';
import 'package:oasis/views/shell.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  testWidgets('shell shows the desktop destination', (tester) async {
    await pumpShell(tester);
    expect(find.byType(OasisShell), findsOneWidget);
    expect(find.byType(DesktopPage), findsOneWidget);
  });

  testWidgets('changing destination swaps the page', (tester) async {
    final state = await pumpShell(tester);
    state.setDestinationIndex(1);
    await tester.pump();
    expect(find.byType(AppsPage), findsOneWidget);

    state.setDestinationIndex(2);
    await tester.pump();
    expect(find.byType(SettingsPage), findsOneWidget);

    state.setDestinationIndex(3);
    await tester.pump();
    expect(find.byType(AboutPage), findsOneWidget);

    state.setDestinationIndex(0);
    await tester.pump();
    expect(find.byType(DesktopPage), findsOneWidget);
  });

  testWidgets('narrow layout still shows the destination',
      (tester) async {
    await pumpShell(tester, surface: const Size(500, 800));
    expect(find.byType(DesktopPage), findsOneWidget);
  });

  testWidgets('platform services factory wires real services',
      (tester) async {
    final services = ShellServices.platform();
    expect(services.entriesLoader, isNotNull);
    expect(services.appLauncher, isNotNull);
    expect(services.batteryReader, isNotNull);
    expect(services.sessionActions, isNotNull);
  });

  testWidgets('app without a platform override', (tester) async {
    await pumpShell(tester, platform: null);
    expect(find.byType(DesktopPage), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
