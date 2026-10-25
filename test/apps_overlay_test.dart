import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/models/desktop_entry.dart';
import 'package:oasis/state/app_state.dart';
import 'package:oasis/views/apps/apps_overlay.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  Future<OasisState> openApps(WidgetTester tester,
      {List<DesktopEntry>? entries, RecordingRunner? runner}) async {
    final state = await pumpShell(
      tester,
      services: fakeServices(entries: entries, runner: runner),
    );
    state.openLauncher();
    await tester.pumpAndSettle();
    await tester.pump();
    return state;
  }

  testWidgets('shows the entries grid over the desktop', (tester) async {
    await openApps(tester);
    expect(find.byType(AppsOverlay), findsOneWidget);
    expect(find.text('Terminal'), findsOneWidget);
    expect(find.text('Files'), findsOneWidget);
    expect(find.text('Web Browser'), findsOneWidget);
  });

  testWidgets('search filters the grid and shows the empty state',
      (tester) async {
    final state = await openApps(tester);
    await tester.enterText(find.byKey(const Key('app_search')), 'term');
    await tester.pump();
    expect(find.text('Terminal'), findsOneWidget);
    expect(find.text('Files'), findsNothing);

    await tester.enterText(find.byKey(const Key('app_search')), 'zzz');
    await tester.pump();
    expect(find.text('No applications found'), findsOneWidget);
    expect(state.launcherQuery, 'zzz');
  });

  testWidgets('tapping an entry launches it and closes the overlay',
      (tester) async {
    final runner = RecordingRunner();
    final state = await openApps(tester, runner: runner);
    await tester.tap(find.text('Terminal'));
    await tester.pumpAndSettle();
    expect(runner.calls, ['gtk-launch org.test.terminal']);
    expect(state.launcherOpen, isFalse);
    expect(find.byType(AppsOverlay), findsNothing);
  });

  testWidgets('failed launch shows a snackbar', (tester) async {
    final runner = RecordingRunner([1, 1]);
    await openApps(tester, runner: runner);
    await tester.tap(find.text('Terminal'));
    await tester.pumpAndSettle();
    expect(runner.calls.length, 2);
    expect(find.text('Could not launch Terminal'), findsOneWidget);
  });

  testWidgets('tapping the backdrop closes the overlay',
      (tester) async {
    final state = await openApps(tester);
    await tester.tapAt(const Offset(600, 400));
    await tester.pumpAndSettle();
    expect(state.launcherOpen, isFalse);
    expect(find.byType(AppsOverlay), findsNothing);
  });

  testWidgets('escape closes the overlay and clears the query',
      (tester) async {
    final state = await openApps(tester);
    await tester.enterText(find.byKey(const Key('app_search')), 'term');
    await tester.pump();
    expect(state.launcherQuery, 'term');

    await tester.sendKeyEvent(LogicalKeyboardKey.escape);
    await tester.pumpAndSettle();
    expect(state.launcherOpen, isFalse);
    expect(state.launcherQuery, '');
    expect(find.byType(AppsOverlay), findsNothing);
  });

  testWidgets('refresh reloads the entries', (tester) async {
    final loader = FakeEntriesLoader(fixtureEntries);
    await openApps(tester);
    // swap: rebuild with the controllable loader
    await tester.pumpWidget(const SizedBox());
    final state = await pumpShell(
      tester,
      services: fakeServices(loader: loader),
    );
    state.openLauncher();
    await tester.pumpAndSettle();
    await tester.pump();
    expect(loader.calls, 1);
    await tester.tap(find.byKey(const Key('apps_refresh')));
    await tester.pump();
    expect(loader.calls, 2);
    expect(find.text('Terminal'), findsOneWidget);
  });
}
