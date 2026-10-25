import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/models/app_update.dart';
import 'package:oasis/state/app_state.dart';
import 'package:package_info_plus/package_info_plus.dart';

import 'helpers.dart';

void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  Future<OasisState> openAbout(
    WidgetTester tester, {
    FakeVersionChecker? checker,
  }) async {
    final state = await pumpShell(tester, versionChecker: checker);
    state.setDestinationIndex(3);
    await tester.pump();
    await tester.pump();
    return state;
  }

  testWidgets('shows icon, title and version', (tester) async {
    PackageInfo.setMockInitialValues(
      appName: 'Oasis',
      packageName: 'gitlab.httpanimations.oasis',
      version: '0.1.0',
      buildNumber: '1',
      buildSignature: 'test',
    );
    await openAbout(tester);
    await tester.pump();
    expect(find.text('Oasis'), findsWidgets);
    expect(find.text('A desktop shell for Wayland'), findsOneWidget);
    expect(find.text('0.1.0'), findsOneWidget);
    expect(find.byType(Image), findsOneWidget);
  });

  testWidgets('up to date result', (tester) async {
    PackageInfo.setMockInitialValues(
      appName: 'Oasis',
      packageName: 'x',
      version: '1.0.0',
      buildNumber: '1',
      buildSignature: 'test',
    );
    final checker = FakeVersionChecker(
      result: const AppUpdate(latestVersion: '0.9.0', updateAvailable: false),
    );
    await openAbout(tester, checker: checker);
    await tester.tap(find.byKey(const Key('check_updates_tile')));
    await tester.pump();
    await tester.pump();
    expect(find.text('Oasis is up to date'), findsOneWidget);
  });

  testWidgets('update available shows release link', (tester) async {
    PackageInfo.setMockInitialValues(
      appName: 'Oasis',
      packageName: 'x',
      version: '1.0.0',
      buildNumber: '1',
      buildSignature: 'test',
    );
    final checker = FakeVersionChecker(
      result: const AppUpdate(
        latestVersion: '2.0.0',
        updateAvailable: true,
        releaseUrl: 'https://gitlab.com/HttpAnimations/oasis/-/releases/v2.0.0',
      ),
    );
    await openAbout(tester, checker: checker);
    await tester.tap(find.byKey(const Key('check_updates_tile')));
    await tester.pump();
    await tester.pump();
    expect(find.text('Version 2.0.0 is available'), findsOneWidget);
    expect(find.text('View release'), findsOneWidget);
    await tester.tap(find.text('View release'));
    await tester.pump();
  });

  testWidgets('failed check shows feedback', (tester) async {
    PackageInfo.setMockInitialValues(
      appName: 'Oasis',
      packageName: 'x',
      version: '1.0.0',
      buildNumber: '1',
      buildSignature: 'test',
    );
    final checker = FakeVersionChecker(error: Exception('offline'));
    await openAbout(tester, checker: checker);
    await tester.tap(find.byKey(const Key('check_updates_tile')));
    await tester.pump();
    await tester.pump();
    expect(find.text('Update check failed'), findsOneWidget);
  });

  testWidgets('checking shows a spinner', (tester) async {
    PackageInfo.setMockInitialValues(
      appName: 'Oasis',
      packageName: 'x',
      version: '1.0.0',
      buildNumber: '1',
      buildSignature: 'test',
    );
    final checker = _NeverChecker();
    await openAbout(tester, checker: checker);
    await tester.tap(find.byKey(const Key('check_updates_tile')));
    await tester.pump();
    expect(find.text('Checking...'), findsOneWidget);
    expect(find.byType(CircularProgressIndicator), findsWidgets);
    await tester.pumpWidget(const SizedBox());
    await tester.pump();
  });

  testWidgets('releases and project links open urls', (tester) async {
    PackageInfo.setMockInitialValues(
      appName: 'Oasis',
      packageName: 'x',
      version: '1.0.0',
      buildNumber: '1',
      buildSignature: 'test',
    );
    final state = await pumpShell(tester);
    state.setDestinationIndex(3);
    await tester.pump();
    await tester.pump();
    await tester.tap(find.text('All releases'));
    await tester.pump();
    await tester.tap(find.text('Project page'));
    await tester.pump();
  });
}

class _NeverChecker extends FakeVersionChecker {
  _NeverChecker() : super();

  @override
  Future<AppUpdate> check(String currentVersion) =>
      Completer<AppUpdate>().future;
}
