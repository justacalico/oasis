import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/l10n/app_localizations.dart';
import 'package:oasis/views/window_controls.dart';

import 'helpers.dart';

void main() {
  setUp(() {
  });

  Future<void> pump(
    WidgetTester tester,
    FakeWindowActions actions, {
    TargetPlatform platform = TargetPlatform.linux,
  }) async {
    await tester.pumpWidget(
      MaterialApp(
        theme: ThemeData(platform: platform),
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(body: WindowControls(actions: actions)),
      ),
    );
  }

  testWidgets('renders three traffic lights on desktop', (tester) async {
    await pump(tester, FakeWindowActions());
    expect(find.byKey(const Key('window_close_button')), findsOneWidget);
    expect(find.byKey(const Key('window_minimize_button')), findsOneWidget);
    expect(find.byKey(const Key('window_maximize_button')), findsOneWidget);
  });

  testWidgets('buttons invoke window actions', (tester) async {
    final actions = FakeWindowActions();
    await pump(tester, actions);
    await tester.tap(find.byKey(const Key('window_close_button')));
    await tester.tap(find.byKey(const Key('window_minimize_button')));
    await tester.tap(find.byKey(const Key('window_maximize_button')));
    expect(actions.calls, ['close', 'minimize', 'toggleMaximize']);
  });

  testWidgets('hover reveals the glyph and enlarges the light',
      (tester) async {
    await pump(tester, FakeWindowActions());
    final light = find.byKey(const Key('window_close_button'));
    expect(find.descendant(of: light, matching: find.byIcon(Icons.close)),
        findsNothing);

    final gesture =
        await tester.createGesture(kind: PointerDeviceKind.mouse);
    await gesture.addPointer(location: Offset.zero);
    addTearDown(gesture.removePointer);
    await gesture.moveTo(tester.getCenter(light));
    await tester.pump();

    expect(find.descendant(of: light, matching: find.byIcon(Icons.close)),
        findsOneWidget);

    await gesture.moveTo(const Offset(400, 400));
    await tester.pump();
    expect(find.descendant(of: light, matching: find.byIcon(Icons.close)),
        findsNothing);
  });

  testWidgets('collapses on non-desktop platforms', (tester) async {
    await pump(tester, FakeWindowActions(),
        platform: TargetPlatform.android);
    expect(find.byKey(const Key('window_close_button')), findsNothing);
    expect(find.byType(SizedBox), findsWidgets);
  });
}
