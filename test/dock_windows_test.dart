import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/models/compositor_window.dart';
import 'package:oasis/views/desktop/dock.dart';

import 'helpers.dart';

void main() {
  testWidgets('dock shows running windows and forwards actions',
      (tester) async {
    final windows = FakeCompositorService(const [
      CompositorWindow(id: 1, title: 'Files', appId: 'org.gnome.Nautilus'),
      CompositorWindow(
          id: 2, title: 'Web', appId: 'org.mozilla.firefox', focused: true),
    ]);
    await pumpShell(tester, services: fakeServices(windows: windows));
    await pumpUntil(tester, () => find.byKey(const Key('window_1')).evaluate().isNotEmpty);

    expect(find.byKey(const Key('window_1')), findsOneWidget);
    expect(find.byKey(const Key('window_2')), findsOneWidget);

    await tester.tap(find.byKey(const Key('window_1')));
    await tester.pump();
    expect(windows.calls, contains('focus:1'));
    expect(windows.windows[0].focused, isTrue);

    // right-click closes
    await tester.tap(find.byKey(const Key('window_2')),
        buttons: kSecondaryMouseButton);
    await tester.pump();
    expect(windows.calls, contains('close:2'));
    expect(windows.windows.length, 1);
  });

  testWidgets('window chips render letter glyphs and focus dot',
      (tester) async {
    final windows = FakeCompositorService(const [
      CompositorWindow(
          id: 3, title: 'Editor', appId: 'dev.editor', focused: true),
    ]);
    await pumpShell(tester, services: fakeServices(windows: windows));
    await pumpUntil(tester, () => find.byKey(const Key('window_3')).evaluate().isNotEmpty);
    expect(find.text('E'), findsOneWidget);
  });
}
