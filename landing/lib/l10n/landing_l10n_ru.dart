// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'landing_l10n.dart';

// ignore_for_file: type=lint

/// The translations for Russian (`ru`).
class LandingL10nRu extends LandingL10n {
  LandingL10nRu([String locale = 'ru']) : super(locale);

  @override
  String get title => 'Oasis';

  @override
  String get tagline => 'Оболочка рабочего стола для Wayland на Flutter';

  @override
  String get download => 'Скачать';

  @override
  String get tryWeb => 'Попробовать в браузере';

  @override
  String get heroText =>
      'Быстрая и красивая оболочка для вашего Linux. Настоящий поиск .desktop приложений, живой бар, управление сессией и лаунчер, который просто работает.';

  @override
  String get featShell => 'Родной Wayland shell';

  @override
  String get featShellSub => 'Панель, лаунчер и управление сессией';

  @override
  String get featLauncher => 'Настоящий лаунчер';

  @override
  String get featLauncherSub => 'Читает установленные приложения';

  @override
  String get featThemes => 'Темы';

  @override
  String get featThemesSub => 'Светлые, тёмные и OLED обои';

  @override
  String get featUpdates => 'Обновления в приложении';

  @override
  String get featUpdatesSub => 'Проверяет релизы GitLab за вас';

  @override
  String get downloads => 'Загрузки';

  @override
  String get releasesOnGitLab => 'Релизы на GitLab';

  @override
  String get flatpakHint => 'Установка Flatpak одной строкой:';

  @override
  String get flatpakRef => 'oasis.flatpakref';

  @override
  String get themeSystem => 'Системная';

  @override
  String get themeLight => 'Светлая';

  @override
  String get themeDark => 'Тёмная';

  @override
  String get themeOled => 'OLED';

  @override
  String get language => 'Язык';

  @override
  String get languageSystem => 'Системный';

  @override
  String get loadingReleases => 'Загрузка релизов...';

  @override
  String get noReleases => 'Пока нет релизов.';

  @override
  String get releaseNotes => 'Примечания к выпуску';

  @override
  String get source => 'Исходный код на GitLab';

  @override
  String get pages => 'GitLab Pages';
}
