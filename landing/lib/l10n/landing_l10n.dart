import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'landing_l10n_en.dart';
import 'landing_l10n_ga.dart';
import 'landing_l10n_ru.dart';
import 'landing_l10n_zh.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of LandingL10n
/// returned by `LandingL10n.of(context)`.
///
/// Applications need to include `LandingL10n.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'l10n/landing_l10n.dart';
///
/// return MaterialApp(
///   localizationsDelegates: LandingL10n.localizationsDelegates,
///   supportedLocales: LandingL10n.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the LandingL10n.supportedLocales
/// property.
abstract class LandingL10n {
  LandingL10n(String locale)
    : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static LandingL10n of(BuildContext context) {
    return Localizations.of<LandingL10n>(context, LandingL10n)!;
  }

  static const LocalizationsDelegate<LandingL10n> delegate =
      _LandingL10nDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates =
      <LocalizationsDelegate<dynamic>>[
        delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
      ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[
    Locale('en'),
    Locale('en', 'GB'),
    Locale('ga'),
    Locale('ru'),
    Locale('zh'),
    Locale.fromSubtags(languageCode: 'zh', scriptCode: 'Hans'),
  ];

  /// No description provided for @title.
  ///
  /// In en, this message translates to:
  /// **'Oasis'**
  String get title;

  /// No description provided for @tagline.
  ///
  /// In en, this message translates to:
  /// **'A desktop environment shell for Wayland, built in Flutter'**
  String get tagline;

  /// No description provided for @download.
  ///
  /// In en, this message translates to:
  /// **'Download'**
  String get download;

  /// No description provided for @tryWeb.
  ///
  /// In en, this message translates to:
  /// **'Try it in your browser'**
  String get tryWeb;

  /// No description provided for @heroText.
  ///
  /// In en, this message translates to:
  /// **'A fast, beautiful desktop shell for your Linux machine. Real .desktop app discovery, a live panel, session controls and a launcher that just works.'**
  String get heroText;

  /// No description provided for @featShell.
  ///
  /// In en, this message translates to:
  /// **'Wayland native shell'**
  String get featShell;

  /// No description provided for @featShellSub.
  ///
  /// In en, this message translates to:
  /// **'Panel, launcher and session controls'**
  String get featShellSub;

  /// No description provided for @featLauncher.
  ///
  /// In en, this message translates to:
  /// **'Real app launcher'**
  String get featLauncher;

  /// No description provided for @featLauncherSub.
  ///
  /// In en, this message translates to:
  /// **'Reads your installed applications'**
  String get featLauncherSub;

  /// No description provided for @featThemes.
  ///
  /// In en, this message translates to:
  /// **'Themeable'**
  String get featThemes;

  /// No description provided for @featThemesSub.
  ///
  /// In en, this message translates to:
  /// **'Light, dark and OLED wallpapers'**
  String get featThemesSub;

  /// No description provided for @featUpdates.
  ///
  /// In en, this message translates to:
  /// **'In-app updates'**
  String get featUpdates;

  /// No description provided for @featUpdatesSub.
  ///
  /// In en, this message translates to:
  /// **'Checks GitLab releases for you'**
  String get featUpdatesSub;

  /// No description provided for @downloads.
  ///
  /// In en, this message translates to:
  /// **'Downloads'**
  String get downloads;

  /// No description provided for @releasesOnGitLab.
  ///
  /// In en, this message translates to:
  /// **'View releases on GitLab'**
  String get releasesOnGitLab;

  /// No description provided for @flatpakHint.
  ///
  /// In en, this message translates to:
  /// **'One-line Flatpak install:'**
  String get flatpakHint;

  /// No description provided for @flatpakRef.
  ///
  /// In en, this message translates to:
  /// **'oasis.flatpakref'**
  String get flatpakRef;

  /// No description provided for @themeSystem.
  ///
  /// In en, this message translates to:
  /// **'System'**
  String get themeSystem;

  /// No description provided for @themeLight.
  ///
  /// In en, this message translates to:
  /// **'Light'**
  String get themeLight;

  /// No description provided for @themeDark.
  ///
  /// In en, this message translates to:
  /// **'Dark'**
  String get themeDark;

  /// No description provided for @themeOled.
  ///
  /// In en, this message translates to:
  /// **'OLED'**
  String get themeOled;

  /// No description provided for @language.
  ///
  /// In en, this message translates to:
  /// **'Language'**
  String get language;

  /// No description provided for @languageSystem.
  ///
  /// In en, this message translates to:
  /// **'System default'**
  String get languageSystem;

  /// No description provided for @loadingReleases.
  ///
  /// In en, this message translates to:
  /// **'Loading releases...'**
  String get loadingReleases;

  /// No description provided for @noReleases.
  ///
  /// In en, this message translates to:
  /// **'No releases yet. Check back soon.'**
  String get noReleases;

  /// No description provided for @releaseNotes.
  ///
  /// In en, this message translates to:
  /// **'Release notes'**
  String get releaseNotes;

  /// No description provided for @source.
  ///
  /// In en, this message translates to:
  /// **'Source on GitLab'**
  String get source;

  /// No description provided for @pages.
  ///
  /// In en, this message translates to:
  /// **'GitLab Pages'**
  String get pages;
}

class _LandingL10nDelegate extends LocalizationsDelegate<LandingL10n> {
  const _LandingL10nDelegate();

  @override
  Future<LandingL10n> load(Locale locale) {
    return SynchronousFuture<LandingL10n>(lookupLandingL10n(locale));
  }

  @override
  bool isSupported(Locale locale) =>
      <String>['en', 'ga', 'ru', 'zh'].contains(locale.languageCode);

  @override
  bool shouldReload(_LandingL10nDelegate old) => false;
}

LandingL10n lookupLandingL10n(Locale locale) {
  // Lookup logic when language+script codes are specified.
  switch (locale.languageCode) {
    case 'zh':
      {
        switch (locale.scriptCode) {
          case 'Hans':
            return LandingL10nZhHans();
        }
        break;
      }
  }

  // Lookup logic when language+country codes are specified.
  switch (locale.languageCode) {
    case 'en':
      {
        switch (locale.countryCode) {
          case 'GB':
            return LandingL10nEnGb();
        }
        break;
      }
  }

  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'en':
      return LandingL10nEn();
    case 'ga':
      return LandingL10nGa();
    case 'ru':
      return LandingL10nRu();
    case 'zh':
      return LandingL10nZh();
  }

  throw FlutterError(
    'LandingL10n.delegate failed to load unsupported locale "$locale". This is likely '
    'an issue with the localizations generation tool. Please file an issue '
    'on GitHub with a reproducible sample app and the gen-l10n configuration '
    'that was used.',
  );
}
