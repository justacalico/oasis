import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/models/desktop_entry.dart';
import 'package:oasis/state/app_state.dart';

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
    state.setDestinationIndex(1);
    await tester.pump();
    await tester.pump();
    return state;
  }

  testWidgets('shows the entries grid', (tester) async {
    await openApps(tester);
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

  testWidgets('tapping an entry launches it', (tester) async {
    final runner = RecordingRunner();
    await openApps(tester, runner: runner);
    await tester.tap(find.text('Terminal'));
    await tester.pump();
    expect(runner.calls, ['gtk-launch org.test.terminal']);
  });

  testWidgets('failed launch shows a snackbar', (tester) async {
    final runner = RecordingRunner([1, 1]);
    await openApps(tester, runner: runner);
    await tester.tap(find.text('Terminal'));
    await tester.pumpAndSettle();
    expect(runner.calls.length, 2);
    expect(find.text('Could not launch Terminal'), findsOneWidget);
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
    state.setDestinationIndex(1);
    await tester.pump();
    await tester.pump();
    expect(loader.calls, 2);
    await tester.tap(find.byKey(const Key('apps_refresh')));
    await tester.pump();
    expect(loader.calls, 3);
    expect(find.text('Terminal'), findsOneWidget);
  });
}
