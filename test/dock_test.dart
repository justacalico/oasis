import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  testWidgets('dock lists applications and launches them', (tester) async {
    final runner = RecordingRunner();
    await pumpShell(tester, services: fakeServices(runner: runner));
    await tester.pump();

    expect(find.byKey(const Key('dock_org.test.terminal')), findsOneWidget);
    expect(find.byKey(const Key('dock_org.test.files')), findsOneWidget);
    expect(find.byKey(const Key('dock_org.test.browser')), findsOneWidget);

    await tester.tap(find.byKey(const Key('dock_org.test.terminal')));
    await tester.pump();
    expect(runner.calls, ['gtk-launch org.test.terminal']);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('dock icons magnify on hover', (tester) async {
    await pumpShell(tester);
    await tester.pump();

    final icon = find.byKey(const Key('dock_org.test.files'));
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

  testWidgets('dock shows the app name in a tooltip', (tester) async {
    await pumpShell(tester);
    await tester.pump();

    final mouse = await tester.createGesture(kind: PointerDeviceKind.mouse);
    await mouse.addPointer(location: Offset.zero);
    addTearDown(mouse.removePointer);

    await mouse.moveTo(
      tester.getCenter(find.byKey(const Key('dock_org.test.browser'))),
    );
    await tester.pump(const Duration(seconds: 1));
    expect(find.text('Web Browser'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('dock apps button opens the applications page', (tester) async {
    final state = await pumpShell(tester);
    await tester.pump();

    await tester.tap(find.byKey(const Key('dock_apps_button')));
    await tester.pump();
    expect(state.destinationIndex, 1);
    expect(find.byKey(const Key('app_search')), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });

  testWidgets('failed dock launch shows a snackbar', (tester) async {
    await pumpShell(
      tester,
      services: fakeServices(exitCodes: const [1, 1]),
    );
    await tester.pump();

    await tester.tap(find.byKey(const Key('dock_org.test.terminal')));
    await tester.pump();
    expect(find.text('Could not launch Terminal'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
