// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Chinese (`zh`).
class AppLocalizationsZh extends AppLocalizations {
  AppLocalizationsZh([String locale = 'zh']) : super(locale);

  @override
  String get appTitle => 'Oasis';

  @override
  String get navDesktop => '桌面';

  @override
  String get navApps => '应用程序';

  @override
  String get navSettings => '设置';

  @override
  String get navAbout => '关于';

  @override
  String get windowClose => '关闭';

  @override
  String get windowMinimize => '最小化';

  @override
  String get windowZoom => '缩放';

  @override
  String get searchApps => '搜索应用程序';

  @override
  String get noApps => '未找到应用程序';

  @override
  String get theme => '主题';

  @override
  String get themeSystem => '跟随系统';

  @override
  String get themeLight => '浅色';

  @override
  String get themeDark => '深色';

  @override
  String get language => '语言';

  @override
  String get languageSystem => '跟随系统';

  @override
  String get wallpaper => '壁纸';

  @override
  String get wallpaperDunes => '沙丘';

  @override
  String get wallpaperLagoon => '泻湖';

  @override
  String get wallpaperMidnight => '午夜';

  @override
  String get quickSettings => '快捷设置';

  @override
  String get sessionLock => '锁定';

  @override
  String get sessionSuspend => '挂起';

  @override
  String get sessionRestart => '重启';

  @override
  String get sessionPowerOff => '关机';

  @override
  String get battery => '电池';

  @override
  String get aboutTagline => 'Wayland 桌面 shell';

  @override
  String get version => '版本';

  @override
  String get checkUpdates => '检查更新';

  @override
  String get checking => '正在检查...';

  @override
  String get upToDate => 'Oasis 已是最新版本';

  @override
  String updateAvailable(String version) {
    return '版本 $version 可用';
  }

  @override
  String get checkFailed => '检查更新失败';

  @override
  String get viewRelease => '查看发布';

  @override
  String get allReleases => '所有发布';

  @override
  String get projectPage => '项目页面';

  @override
  String launchFailed(String name) {
    return '无法启动 $name';
  }

  @override
  String get refresh => '刷新';
}

/// The translations for Chinese, using the Han script (`zh_Hans`).
class AppLocalizationsZhHans extends AppLocalizationsZh {
  AppLocalizationsZhHans() : super('zh_Hans');
}
