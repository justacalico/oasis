import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/services/battery.dart';
import 'package:package_info_plus/package_info_plus.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
    PackageInfo.setMockInitialValues(
      appName: 'Oasis',
      packageName: 'gitlab.httpanimations.oasis',
      version: '0.1.0',
      buildNumber: '1',
      buildSignature: 'test',
    );
  });

  Future<void> snap(WidgetTester tester, String name) async {
    await tester.pump();
    await expectLater(
      find.byType(MaterialApp),
      matchesGoldenFile('goldens/$name.png'),
    );
  }

  testWidgets('desktop shell', (tester) async {
    await pumpShell(
      tester,
      services: fakeServices(
        battery: const BatteryStatus(capacity: 87, charging: true),
      ),
    );
    await snap(tester, 'desktop');
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('applications', (tester) async {
    final state = await pumpShell(tester);
    state.setDestinationIndex(1);
    await tester.pump();
    await snap(tester, 'apps');
  });

  testWidgets('settings', (tester) async {
    final state = await pumpShell(tester);
    state.setDestinationIndex(2);
    await snap(tester, 'settings');
  });

  testWidgets('about', (tester) async {
    final state = await pumpShell(tester);
    state.setDestinationIndex(3);
    await tester.pump();
    await snap(tester, 'about');
  });

  testWidgets('quick settings', (tester) async {
    await pumpShell(tester);
    await tester.tap(find.byKey(const Key('quick_settings_button')));
    await tester.pumpAndSettle();
    await snap(tester, 'quick_settings');
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('narrow layout with drawer', (tester) async {
    await pumpShell(tester, surface: const Size(500, 800));
    await tester.tap(find.byKey(const Key('drawer_button')));
    await tester.pump();
    await pumpUntil(
      tester,
      () =>
          tester.any(find.text('About')) &&
          tester.getCenter(find.text('About')).dx > 0,
    );
    await snap(tester, 'narrow_drawer');
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('zh settings golden', (tester) async {
    final state = await pumpShell(
      tester,
      locale: const Locale.fromSubtags(
          languageCode: 'zh', scriptCode: 'Hans'),
    );
    state.setDestinationIndex(2);
    await snap(tester, 'settings_zh');
  });

  testWidgets('ru apps golden', (tester) async {
    final state = await pumpShell(tester, locale: const Locale('ru'));
    state.setDestinationIndex(1);
    await tester.pump();
    await snap(tester, 'apps_ru');
  });

  testWidgets('ga about golden', (tester) async {
    final state = await pumpShell(tester, locale: const Locale('ga'));
    state.setDestinationIndex(3);
    await tester.pump();
    await snap(tester, 'about_ga');
  });
}
