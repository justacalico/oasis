import 'dart:async';

import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/app_update.dart';
import '../services/version_checker.dart';

enum Wallpaper { dunes, lagoon, midnight }

enum AppDestination { desktop, apps, settings, about }

/// Shared application state: theme, locale, navigation and the update check.
class OasisState extends ChangeNotifier {
  final SharedPreferences _prefs;
  final VersionChecker _versionChecker;

  OasisState(this._prefs, {VersionChecker? versionChecker})
      : _versionChecker = versionChecker ?? VersionChecker() {
    _themeMode = ThemeMode
        .values[_prefs.getInt('themeMode') ?? ThemeMode.system.index];
    final code = _prefs.getString('locale');
    if (code != null) {
      final parts = code.split('_');
      _locale = parts.length == 2
          ? Locale.fromSubtags(languageCode: parts[0], scriptCode: parts[1])
          : Locale(parts[0]);
    }
    _wallpaper =
        Wallpaper.values[_prefs.getInt('wallpaper') ?? Wallpaper.dunes.index];
  }

  ThemeMode _themeMode = ThemeMode.system;
  ThemeMode get themeMode => _themeMode;

  void setThemeMode(ThemeMode mode) {
    if (mode == _themeMode) return;
    _themeMode = mode;
    _prefs.setInt('themeMode', mode.index);
    notifyListeners();
  }

  Locale? _locale;
  Locale? get locale => _locale;

  static const supportedLocales = [
    Locale('en', 'GB'),
    Locale('zh', 'Hans'),
    Locale('ru'),
    Locale('ga'),
  ];

  void setLocale(Locale? locale) {
    if (locale == _locale) return;
    _locale = locale;
    if (locale == null) {
      _prefs.remove('locale');
    } else {
      _prefs.setString(
        'locale',
        locale.scriptCode != null
            ? '${locale.languageCode}_${locale.scriptCode}'
            : locale.languageCode,
      );
    }
    notifyListeners();
  }

  Wallpaper _wallpaper = Wallpaper.dunes;
  Wallpaper get wallpaper => _wallpaper;

  void setWallpaper(Wallpaper wallpaper) {
    if (wallpaper == _wallpaper) return;
    _wallpaper = wallpaper;
    _prefs.setInt('wallpaper', wallpaper.index);
    notifyListeners();
  }

  int _destinationIndex = 0;
  int get destinationIndex => _destinationIndex;

  void setDestinationIndex(int index) {
    if (index == _destinationIndex) return;
    _destinationIndex = index;
    notifyListeners();
  }

  String _launcherQuery = '';
  String get launcherQuery => _launcherQuery;

  void setLauncherQuery(String query) {
    if (query == _launcherQuery) return;
    _launcherQuery = query;
    notifyListeners();
  }

  AppUpdate? _appUpdate;
  AppUpdate? get appUpdate => _appUpdate;

  bool _checkingUpdate = false;
  bool get checkingUpdate => _checkingUpdate;

  Future<AppUpdate?>? _updateCheck;

  /// Runs an update check, de-duplicating concurrent calls. Returns null when
  /// the check fails so callers can show feedback.
  Future<AppUpdate?> checkForAppUpdate(String currentVersion) {
    final existing = _updateCheck;
    if (existing != null) return existing;
    return _updateCheck =
        _doCheck(currentVersion).whenComplete(() => _updateCheck = null);
  }

  Future<AppUpdate?> _doCheck(String currentVersion) async {
    _checkingUpdate = true;
    notifyListeners();
    try {
      final update = await _versionChecker.check(currentVersion);
      if (_appUpdate != update) {
        _appUpdate = update;
      }
      return update;
    } catch (_) {
      return null;
    } finally {
      _checkingUpdate = false;
      notifyListeners();
    }
  }
}

/// A [VersionChecker] that never touches the network, for tests and the web
/// demo.
class NoNetworkVersionChecker extends VersionChecker {
  final AppUpdate result;

  NoNetworkVersionChecker({this.result = const AppUpdate()})
      : super(client: null);

  @override
  Future<AppUpdate> check(String currentVersion) async =>
      result.copyWith(currentVersion: currentVersion);
}
