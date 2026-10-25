// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'landing_l10n.dart';

// ignore_for_file: type=lint

/// The translations for Irish (`ga`).
class LandingL10nGa extends LandingL10n {
  LandingL10nGa([String locale = 'ga']) : super(locale);

  @override
  String get title => 'Oasis';

  @override
  String get tagline =>
      'Blaosc timpeallacht deisce do Wayland, tógtha in Flutter';

  @override
  String get download => 'Íoslódáil';

  @override
  String get tryWeb => 'Bain triail as sa bhrabhsálaí';

  @override
  String get heroText =>
      'Blaosc deisce gasta álainn do do mheaisín Linux. Fíor-aimsitheoir .desktop, painéal beo, rialuithe seisiúin agus lainseálaí a oibríonn.';

  @override
  String get featShell => 'Blaosc dhúchais Wayland';

  @override
  String get featShellSub => 'Painéal, lainseálaí agus rialuithe seisiúin';

  @override
  String get featLauncher => 'Fíor-lainseálaí';

  @override
  String get featLauncherSub => 'Léann sé d\'fheidhmchláir shuiteáilte';

  @override
  String get featThemes => 'Téamaí';

  @override
  String get featThemesSub => 'Páipéar balla solas, dorcha agus OLED';

  @override
  String get featUpdates => 'Nuashonruithe san aip';

  @override
  String get featUpdatesSub => 'Seiceálann sé eisiúintí GitLab duit';

  @override
  String get downloads => 'Íoslódálacha';

  @override
  String get releasesOnGitLab => 'Amharc ar eisiúintí ar GitLab';

  @override
  String get flatpakHint => 'Suiteáil Flatpak in aon líne amháin:';

  @override
  String get flatpakRef => 'oasis.flatpakref';

  @override
  String get themeSystem => 'Córas';

  @override
  String get themeLight => 'Solas';

  @override
  String get themeDark => 'Dorcha';

  @override
  String get themeOled => 'OLED';

  @override
  String get language => 'Teanga';

  @override
  String get languageSystem => 'Réamhshocrú an chórais';

  @override
  String get loadingReleases => 'Ag lódáil eisiúintí...';

  @override
  String get noReleases => 'Níl aon eisiúint fós.';

  @override
  String get releaseNotes => 'Nótaí eisiúna';

  @override
  String get source => 'Foinse ar GitLab';

  @override
  String get pages => 'GitLab Pages';
}
