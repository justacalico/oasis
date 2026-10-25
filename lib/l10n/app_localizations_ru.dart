// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Russian (`ru`).
class AppLocalizationsRu extends AppLocalizations {
  AppLocalizationsRu([String locale = 'ru']) : super(locale);

  @override
  String get appTitle => 'Oasis';

  @override
  String get navApps => 'Приложения';

  @override
  String get searchApps => 'Поиск приложений';

  @override
  String get noApps => 'Приложения не найдены';

  @override
  String get theme => 'Тема';

  @override
  String get themeSystem => 'Системная';

  @override
  String get themeLight => 'Светлая';

  @override
  String get themeDark => 'Тёмная';

  @override
  String get language => 'Язык';

  @override
  String get languageSystem => 'Системный';

  @override
  String get wallpaper => 'Обои';

  @override
  String get wallpaperDunes => 'Дюны';

  @override
  String get wallpaperLagoon => 'Лагуна';

  @override
  String get wallpaperMidnight => 'Полночь';

  @override
  String get quickSettings => 'Быстрые настройки';

  @override
  String get sessionLock => 'Блокировка';

  @override
  String get sessionSuspend => 'Сон';

  @override
  String get sessionRestart => 'Перезагрузка';

  @override
  String get sessionPowerOff => 'Выключение';

  @override
  String get battery => 'Батарея';

  @override
  String get aboutTagline => 'Оболочка рабочего стола для Wayland';

  @override
  String get version => 'Версия';

  @override
  String get checkUpdates => 'Проверить обновления';

  @override
  String get checking => 'Проверка...';

  @override
  String get upToDate => 'Oasis обновлён';

  @override
  String updateAvailable(String version) {
    return 'Доступна версия $version';
  }

  @override
  String get checkFailed => 'Не удалось проверить обновления';

  @override
  String get viewRelease => 'Открыть релиз';

  @override
  String get allReleases => 'Все релизы';

  @override
  String get projectPage => 'Страница проекта';

  @override
  String launchFailed(String name) {
    return 'Не удалось запустить $name';
  }

  @override
  String get refresh => 'Обновить';
}
