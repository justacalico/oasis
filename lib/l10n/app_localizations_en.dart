// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class AppLocalizationsEn extends AppLocalizations {
  AppLocalizationsEn([String locale = 'en']) : super(locale);

  @override
  String get appTitle => 'Oasis';

  @override
  String get navApps => 'Applications';

  @override
  String get searchApps => 'Search applications';

  @override
  String get noApps => 'No applications found';

  @override
  String get theme => 'Theme';

  @override
  String get themeSystem => 'System';

  @override
  String get themeLight => 'Light';

  @override
  String get themeDark => 'Dark';

  @override
  String get language => 'Language';

  @override
  String get languageSystem => 'System default';

  @override
  String get wallpaper => 'Wallpaper';

  @override
  String get wallpaperDunes => 'Dunes';

  @override
  String get wallpaperLagoon => 'Lagoon';

  @override
  String get wallpaperMidnight => 'Midnight';

  @override
  String get quickSettings => 'Quick settings';

  @override
  String get sessionLock => 'Lock';

  @override
  String get sessionSuspend => 'Suspend';

  @override
  String get sessionRestart => 'Restart';

  @override
  String get sessionPowerOff => 'Power off';

  @override
  String get battery => 'Battery';

  @override
  String get aboutTagline => 'A desktop shell for Wayland';

  @override
  String get version => 'Version';

  @override
  String get checkUpdates => 'Check for updates';

  @override
  String get checking => 'Checking...';

  @override
  String get upToDate => 'Oasis is up to date';

  @override
  String updateAvailable(String version) {
    return 'Version $version is available';
  }

  @override
  String get checkFailed => 'Update check failed';

  @override
  String get viewRelease => 'View release';

  @override
  String get allReleases => 'All releases';

  @override
  String get projectPage => 'Project page';

  @override
  String launchFailed(String name) {
    return 'Could not launch $name';
  }

  @override
  String get refresh => 'Refresh';
}

/// The translations for English, as used in the United Kingdom (`en_GB`).
class AppLocalizationsEnGb extends AppLocalizationsEn {
  AppLocalizationsEnGb() : super('en_GB');
}
