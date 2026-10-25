import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/l10n/app_localizations.dart';
import 'package:oasis/models/desktop_entry.dart';
import 'package:oasis/views/desktop/dock.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  testWidgets('dock renders the entries it is given', (tester) async {
    final launched = <DesktopEntry>[];
    await tester.pumpWidget(
      MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          body: Center(
            child: OasisDock(
              entries: fixtureEntries,
              onLaunch: launched.add,
              onShowApps: () {},
            ),
          ),
        ),
      ),
    );

    expect(find.byKey(const Key('dock_org.test.terminal')), findsOneWidget);
    expect(find.byKey(const Key('dock_org.test.files')), findsOneWidget);
    expect(find.byKey(const Key('dock_org.test.browser')), findsOneWidget);

    await tester.tap(find.byKey(const Key('dock_org.test.terminal')));
    expect(launched.single.id, 'org.test.terminal');
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('shell dock starts empty', (tester) async {
    await pumpShell(tester);
    await tester.pump();

    expect(find.byKey(const Key('dock_apps_button')), findsOneWidget);
    expect(find.byKey(const Key('dock_org.test.terminal')), findsNothing);
    expect(find.byKey(const Key('dock_org.test.files')), findsNothing);
    expect(find.byKey(const Key('dock_org.test.browser')), findsNothing);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('dock icons magnify on hover', (tester) async {
    await pumpShell(tester);
    await tester.pump();

    final icon = find.byKey(const Key('dock_apps_button'));
    final scale = find.descendant(
      of: icon,
      matching: find.byType(AnimatedScale),
    );
    expect(tester.widget<AnimatedScale>(scale).scale, 1.0);

    final mouse = await tester.createGesture(kind: PointerDeviceKind.mouse);
    await mouse.addPointer(location: Offset.zero);
    addTearDown(mouse.removePointer);

    await mouse.moveTo(tester.getCenter(icon));
    await tester.pump();
    expect(tester.widget<AnimatedScale>(scale).scale, 1.25);

    await mouse.moveTo(Offset.zero);
    await tester.pump();
    expect(tester.widget<AnimatedScale>(scale).scale, 1.0);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('dock shows a tooltip on hover', (tester) async {
    await pumpShell(tester);
    await tester.pump();

    final mouse = await tester.createGesture(kind: PointerDeviceKind.mouse);
    await mouse.addPointer(location: Offset.zero);
    addTearDown(mouse.removePointer);

    await mouse.moveTo(
      tester.getCenter(find.byKey(const Key('dock_apps_button'))),
    );
    await tester.pump(const Duration(seconds: 1));
    expect(find.text('Applications'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('dock apps button opens the applications overlay',
      (tester) async {
    final state = await pumpShell(tester);
    await tester.pump();

    await tester.tap(find.byKey(const Key('dock_apps_button')));
    await tester.pump();
    expect(state.launcherOpen, isTrue);
    expect(find.byKey(const Key('app_search')), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
