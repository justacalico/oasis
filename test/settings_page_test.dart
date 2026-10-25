import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/state/app_state.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  Future<OasisState> openSettings(WidgetTester tester,
      {Map<String, Object> prefs = const {}}) async {
    final state = await pumpShell(tester, prefs: prefs);
    state.setDestinationIndex(2);
    await tester.pump();
    return state;
  }

  testWidgets('theme segmented button switches modes', (tester) async {
    final state = await openSettings(tester);
    await tester.tap(find.text('Dark'));
    await tester.pump();
    expect(state.themeMode, ThemeMode.dark);
    await tester.tap(find.text('Light'));
    await tester.pump();
    expect(state.themeMode, ThemeMode.light);
    await tester.tap(find.text('System'));
    await tester.pump();
    expect(state.themeMode, ThemeMode.system);
  });

  testWidgets('language picker lists every language', (tester) async {
    final state = await openSettings(tester);
    await tester.tap(find.byKey(const Key('language_picker')));
    await tester.pumpAndSettle();
    for (final label in [
      'System default',
      'Traditional English',
      '简体中文',
      'Русский',
      'Gaeilge',
    ]) {
      expect(find.text(label), findsWidgets, reason: label);
    }
    await tester.tap(find.text('Gaeilge').last);
    await tester.pumpAndSettle();
    expect(state.locale, const Locale('ga'));
    expect(find.text('Teanga'), findsOneWidget);
  });

  testWidgets('wallpaper rows select the wallpaper', (tester) async {
    final state = await openSettings(tester);
    await tester.tap(find.text('Lagoon'));
    await tester.pump();
    expect(state.wallpaper, Wallpaper.lagoon);
    await tester.tap(find.text('Midnight'));
    await tester.pump();
    expect(state.wallpaper, Wallpaper.midnight);
  });
}
