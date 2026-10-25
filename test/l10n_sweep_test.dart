import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/l10n/app_localizations.dart';
import 'package:oasis/models/app_update.dart';
import 'package:oasis/services/battery.dart';
import 'package:oasis/views/about/about_page.dart';
import 'package:oasis/views/desktop/quick_settings.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'helpers.dart';

// Exercises every localized string in every supported locale so generated
// translations and hardcoded strings are caught, and generated l10n code is
// fully covered.
void main() {
  setUp(() async {
    await loadOasisFonts();
  });

  const locales = [
    Locale('en'),
    Locale('en', 'GB'),
    Locale('zh'),
    Locale.fromSubtags(languageCode: 'zh', scriptCode: 'Hans'),
    Locale('ru'),
    Locale('ga'),
  ];

  for (final locale in locales) {
    testWidgets('all screens in $locale', (tester) async {
      final runner = RecordingRunner([1, 1]);
      final state = await pumpShell(
        tester,
        services: fakeServices(
          battery: const BatteryStatus(capacity: 42, charging: true),
          runner: runner,
        ),
      );
      state.setLocale(locale);
      await tester.pump();

      // Desktop: panel + quick settings.
      await tester.tap(find.byKey(const Key('quick_settings_button')));
      await tester.pumpAndSettle();
      await tester.tap(find.byKey(const Key('session_lock')));
      await tester.pumpAndSettle();

      // Apps overlay: grid, search empty state, failed launch snackbar.
      state.openLauncher();
      await tester.pumpAndSettle();
      await tester.enterText(find.byKey(const Key('app_search')), 'zzz');
      await tester.pump();
      await tester.enterText(find.byKey(const Key('app_search')), '');
      await tester.pump();
      await tester.tap(find.text('Terminal'));
      await tester.pumpAndSettle();

      // Settings + About.
      state.setDestinationIndex(1);
      await tester.pump();
      state.setDestinationIndex(2);
      await tester.pump();
      await tester.pump();

      await tester.pumpWidget(const SizedBox());
    });
  }

  for (final locale in locales) {
    testWidgets('update tile states in $locale', (tester) async {
      SharedPreferences.setMockInitialValues({});
      Future<void> pumpTile({
        bool checking = false,
        bool failed = false,
        AppUpdate? update,
      }) async {
        await tester.pumpWidget(
          MaterialApp(
            locale: locale,
            localizationsDelegates:
                AppLocalizations.localizationsDelegates,
            supportedLocales: AppLocalizations.supportedLocales,
            home: Scaffold(
              body: UpdateTile(
                checking: checking,
                failed: failed,
                update: update,
                onCheck: () {},
                onOpenUrl: (uri) async => true,
              ),
            ),
          ),
        );
        await tester.pump();
      }

      await pumpTile();
      await pumpTile(checking: true);
      await pumpTile(failed: true);
      await pumpTile(
        update: const AppUpdate(latestVersion: '9.9.9'),
      );
      await pumpTile(
        update: const AppUpdate(
          latestVersion: '9.9.9',
          updateAvailable: true,
          releaseUrl: 'https://example.com/v9.9.9',
        ),
      );
      // Open the release link on the update-available state.
      await tester.tap(find.byKey(const Key('view_release_tile')));
      await tester.pump();
    });
  }

  test('l10n delegate throws for an unsupported locale', () {
    expect(
      () => AppLocalizations.delegate.load(const Locale('fr')),
      throwsFlutterError,
    );
  });

  testWidgets('quick settings panel standalone', (tester) async {
    SessionAction? chosen;
    await tester.pumpWidget(
      MaterialApp(
        localizationsDelegates: AppLocalizations.localizationsDelegates,
        supportedLocales: AppLocalizations.supportedLocales,
        home: Scaffold(
          body: QuickSettingsPanel(
            title: 'Quick settings',
            onAction: (a) => chosen = a,
          ),
        ),
      ),
    );
    await tester.tap(find.byKey(const Key('session_powerOff')));
    expect(chosen, SessionAction.powerOff);
  });
}
