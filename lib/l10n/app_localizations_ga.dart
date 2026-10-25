// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Irish (`ga`).
class AppLocalizationsGa extends AppLocalizations {
  AppLocalizationsGa([String locale = 'ga']) : super(locale);

  @override
  String get appTitle => 'Oasis';

  @override
  String get navDesktop => 'Deasc';

  @override
  String get navApps => 'Feidhmchláir';

  @override
  String get navSettings => 'Socruithe';

  @override
  String get navAbout => 'Faoi';

  @override
  String get searchApps => 'Cuardaigh feidhmchláir';

  @override
  String get noApps => 'Níor aimsíodh feidhmchláir';

  @override
  String get theme => 'Téama';

  @override
  String get themeSystem => 'Córas';

  @override
  String get themeLight => 'Solas';

  @override
  String get themeDark => 'Dorcha';

  @override
  String get language => 'Teanga';

  @override
  String get languageSystem => 'Réamhshocrú an chórais';

  @override
  String get wallpaper => 'Páipéar balla';

  @override
  String get wallpaperDunes => 'Dumhcha';

  @override
  String get wallpaperLagoon => 'Lagún';

  @override
  String get wallpaperMidnight => 'Meán oíche';

  @override
  String get quickSettings => 'Socruithe gasta';

  @override
  String get sessionLock => 'Glasáil';

  @override
  String get sessionSuspend => 'Fionraí';

  @override
  String get sessionRestart => 'Atosaigh';

  @override
  String get sessionPowerOff => 'Múch';

  @override
  String get battery => 'Cadhnra';

  @override
  String get aboutTagline => 'Blaosc deisce do Wayland';

  @override
  String get version => 'Leagan';

  @override
  String get checkUpdates => 'Seiceáil nuashonruithe';

  @override
  String get checking => 'Á sheiceáil...';

  @override
  String get upToDate => 'Tá Oasis cothrom le dáta';

  @override
  String updateAvailable(String version) {
    return 'Tá leagan $version ar fáil';
  }

  @override
  String get checkFailed => 'Theip ar sheiceáil nuashonruithe';

  @override
  String get viewRelease => 'Amharc ar an eisiúint';

  @override
  String get allReleases => 'Gach eisiúint';

  @override
  String get projectPage => 'Leathanach an tionscadail';

  @override
  String launchFailed(String name) {
    return 'Níorbh fhéidir $name a sheoladh';
  }

  @override
  String get refresh => 'Athnuaigh';
}
