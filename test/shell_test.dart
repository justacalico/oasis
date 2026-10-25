import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/views/about/about_page.dart';
import 'package:oasis/views/apps/apps_page.dart';
import 'package:oasis/views/settings/settings_page.dart';
import 'package:oasis/views/shell.dart';
import 'package:oasis/views/sidebar.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  testWidgets('wide shell shows sidebar and desktop', (tester) async {
    await pumpShell(tester);
    expect(find.byType(OasisSidebar), findsOneWidget);
    expect(find.text('Desktop'), findsOneWidget);
    expect(find.text('Applications'), findsOneWidget);
    expect(find.text('Settings'), findsOneWidget);
    expect(find.text('About'), findsOneWidget);
  });

  testWidgets('tapping a row navigates to that destination', (tester) async {
    await pumpShell(tester);
    await tester.tap(find.text('Settings'));
    await tester.pump();
    expect(find.byType(SettingsPage), findsOneWidget);

    await tester.tap(find.text('Applications'));
    await tester.pumpAndSettle();
    expect(find.byType(AppsPage), findsOneWidget);

    await tester.tap(find.text('About'));
    await tester.pump();
    expect(find.byType(AboutPage), findsOneWidget);
  });

  testWidgets('narrow shell collapses the sidebar into a drawer',
      (tester) async {
    await pumpShell(tester, surface: const Size(500, 800));
    expect(find.byType(OasisSidebar), findsNothing);
    expect(find.byKey(const Key('drawer_button')), findsOneWidget);

    await tester.tap(find.byKey(const Key('drawer_button')));
    await tester.pump();
    await pumpUntil(
      tester,
      () =>
          tester.any(find.text('About')) &&
          tester.getCenter(find.text('About')).dx > 0,
    );
    expect(find.byType(OasisSidebar), findsOneWidget);

    await tester.tap(find.text('About'));
    await tester.pump();
    await pumpUntil(
        tester, () => !tester.any(find.byType(OasisSidebar)));
    expect(find.byType(OasisSidebar), findsNothing);
    expect(find.byType(AboutPage), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('non-desktop keeps the sidebar', (tester) async {
    await pumpShell(tester, platform: TargetPlatform.android);
    expect(find.byType(OasisSidebar), findsOneWidget);
  });

  testWidgets('non-desktop narrow shows a plain header row', (tester) async {
    await pumpShell(
      tester,
      surface: const Size(500, 800),
      platform: TargetPlatform.android,
    );
    expect(find.byKey(const Key('drawer_button')), findsOneWidget);
    await tester.tap(find.byKey(const Key('drawer_button')));
    await tester.pump();
    await pumpUntil(
      tester,
      () =>
          tester.any(find.text('About')) &&
          tester.getCenter(find.text('About')).dx > 0,
    );
    expect(find.byType(OasisSidebar), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('platform services factory wires real services', (tester) async {
    final services = ShellServices.platform();
    expect(services.entriesLoader, isNotNull);
    expect(services.appLauncher, isNotNull);
    expect(services.batteryReader, isNotNull);
    expect(services.sessionActions, isNotNull);
  });

  testWidgets('app without a platform override', (tester) async {
    await pumpShell(tester, platform: null);
    expect(find.byType(OasisSidebar), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
