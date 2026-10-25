import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/models/app_update.dart';
import 'package:oasis/state/app_state.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'helpers.dart';

Future<(OasisState, SharedPreferences)> makeState(
    [Map<String, Object> prefs = const {}]) async {
  SharedPreferences.setMockInitialValues(prefs);
  final store = await SharedPreferences.getInstance();
  return (OasisState(store), store);
}

void main() {
  test('defaults to system theme, system locale, dunes wallpaper', () async {
    final (state, _) = await makeState();
    expect(state.themeMode, ThemeMode.system);
    expect(state.locale, isNull);
    expect(state.wallpaper, Wallpaper.dunes);
    expect(state.destinationIndex, 0);
    expect(state.launcherQuery, '');
  });

  test('restores persisted settings', () async {
    final (state, _) = await makeState({
      'themeMode': ThemeMode.dark.index,
      'locale': 'zh_Hans',
      'wallpaper': Wallpaper.midnight.index,
    });
    expect(state.themeMode, ThemeMode.dark);
    expect(state.locale,
        const Locale.fromSubtags(languageCode: 'zh', scriptCode: 'Hans'));
    expect(state.wallpaper, Wallpaper.midnight);
  });

  test('restores language-only locale', () async {
    final (state, _) = await makeState({'locale': 'ru'});
    expect(state.locale, const Locale('ru'));
  });

  test('persists and notifies on theme change', () async {
    final (state, store) = await makeState();
    var notified = 0;
    state.addListener(() => notified++);
    state.setThemeMode(ThemeMode.dark);
    expect(state.themeMode, ThemeMode.dark);
    expect(notified, 1);
    state.setThemeMode(ThemeMode.dark);
    expect(notified, 1);

    final restored = OasisState(store);
    expect(restored.themeMode, ThemeMode.dark);
  });

  test('persists locale and clears it on null', () async {
    final (state, store) = await makeState();
    state.setLocale(const Locale('ga'));
    expect(state.locale, const Locale('ga'));

    final restored = OasisState(store);
    expect(restored.locale, const Locale('ga'));

    state.setLocale(null);
    expect(state.locale, isNull);
    final cleared = OasisState(store);
    expect(cleared.locale, isNull);
  });

  test('persists wallpaper', () async {
    final (state, store) = await makeState();
    state.setWallpaper(Wallpaper.lagoon);
    expect(state.wallpaper, Wallpaper.lagoon);
    state.setWallpaper(Wallpaper.lagoon);
    final restored = OasisState(store);
    expect(restored.wallpaper, Wallpaper.lagoon);
  });

  test('tracks navigation and launcher query', () async {
    final (state, _) = await makeState();
    state.setDestinationIndex(2);
    expect(state.destinationIndex, 2);
    state.setDestinationIndex(2);
    state.setLauncherQuery('term');
    expect(state.launcherQuery, 'term');
    state.setLauncherQuery('term');
  });

  group('update check', () {
    test('caches the result and notifies', () async {
      SharedPreferences.setMockInitialValues({});
      final store = await SharedPreferences.getInstance();
      final checker = FakeVersionChecker(
        result: const AppUpdate(
          latestVersion: '2.0.0',
          updateAvailable: true,
          releaseUrl: 'https://example.com/v2',
        ),
      );
      final state = OasisState(store, versionChecker: checker);
      var notified = 0;
      state.addListener(() => notified++);

      expect(state.checkingUpdate, isFalse);
      final update = await state.checkForAppUpdate('1.0.0');
      expect(update!.latestVersion, '2.0.0');
      expect(state.appUpdate!.updateAvailable, isTrue);
      expect(state.checkingUpdate, isFalse);
      expect(notified, greaterThan(0));
      expect(checker.calls, 1);
    });

    test('de-duplicates concurrent checks', () async {
      SharedPreferences.setMockInitialValues({});
      final store = await SharedPreferences.getInstance();
      final checker = FakeVersionChecker();
      final state = OasisState(store, versionChecker: checker);
      final a = state.checkForAppUpdate('1.0.0');
      final b = state.checkForAppUpdate('1.0.0');
      await Future.wait([a, b]);
      expect(checker.calls, 1);
    });

    test('returns null when the checker throws', () async {
      SharedPreferences.setMockInitialValues({});
      final store = await SharedPreferences.getInstance();
      final checker = FakeVersionChecker(error: Exception('offline'));
      final state = OasisState(store, versionChecker: checker);
      expect(await state.checkForAppUpdate('1.0.0'), isNull);
      expect(state.appUpdate, isNull);
      expect(state.checkingUpdate, isFalse);
    });

    test('does not replace identical cached update', () async {
      SharedPreferences.setMockInitialValues({});
      final store = await SharedPreferences.getInstance();
      const same = AppUpdate(latestVersion: '1.0.0', releaseUrl: 'u');
      final checker = FakeVersionChecker(result: same);
      final state = OasisState(store, versionChecker: checker);
      await state.checkForAppUpdate('1.0.0');
      final first = state.appUpdate;
      await state.checkForAppUpdate('1.0.0');
      expect(identical(state.appUpdate, first), isTrue);
    });
  });

  test('NoNetworkVersionChecker never throws', () async {
    final checker = NoNetworkVersionChecker(
      result: const AppUpdate(latestVersion: '3.0.0'),
    );
    final update = await checker.check('1.0.0');
    expect(update.latestVersion, '3.0.0');
    expect(update.currentVersion, '1.0.0');
  });
}