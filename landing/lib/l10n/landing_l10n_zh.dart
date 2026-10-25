// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'landing_l10n.dart';

// ignore_for_file: type=lint

/// The translations for Chinese (`zh`).
class LandingL10nZh extends LandingL10n {
  LandingL10nZh([String locale = 'zh']) : super(locale);

  @override
  String get title => 'Oasis';

  @override
  String get tagline => '用 Flutter 构建的 Wayland 桌面环境 shell';

  @override
  String get download => '下载';

  @override
  String get tryWeb => '在浏览器中试用';

  @override
  String get heroText =>
      '为你的 Linux 机器打造的快速美观桌面 shell。真实的 .desktop 应用发现、实时面板、会话控制和开箱即用的启动器。';

  @override
  String get featShell => '原生 Wayland shell';

  @override
  String get featShellSub => '面板、启动器和会话控制';

  @override
  String get featLauncher => '真实应用启动器';

  @override
  String get featLauncherSub => '读取已安装的应用程序';

  @override
  String get featThemes => '主题化';

  @override
  String get featThemesSub => '浅色、深色和 OLED 壁纸';

  @override
  String get featUpdates => '应用内更新';

  @override
  String get featUpdatesSub => '自动检查 GitLab 发布';

  @override
  String get downloads => '下载';

  @override
  String get releasesOnGitLab => '在 GitLab 查看发布';

  @override
  String get flatpakHint => '一行 Flatpak 安装:';

  @override
  String get flatpakRef => 'oasis.flatpakref';

  @override
  String get themeSystem => '跟随系统';

  @override
  String get themeLight => '浅色';

  @override
  String get themeDark => '深色';

  @override
  String get themeOled => 'OLED';

  @override
  String get language => '语言';

  @override
  String get languageSystem => '跟随系统';

  @override
  String get loadingReleases => '正在加载发布...';

  @override
  String get noReleases => '暂无发布，敬请期待。';

  @override
  String get releaseNotes => '发布说明';

  @override
  String get source => 'GitLab 源码';

  @override
  String get pages => 'GitLab Pages';
}

/// The translations for Chinese, using the Han script (`zh_Hans`).
class LandingL10nZhHans extends LandingL10nZh {
  LandingL10nZhHans() : super('zh_Hans');

  @override
  String get title => 'Oasis';

  @override
  String get tagline =>
      'A desktop environment shell for Wayland, built in Flutter';

  @override
  String get download => 'Download';

  @override
  String get tryWeb => 'Try it in your browser';

  @override
  String get heroText =>
      'A fast, beautiful desktop shell for your Linux machine. Real .desktop app discovery, a live panel, session controls and a launcher that just works.';

  @override
  String get featShell => 'Wayland native shell';

  @override
  String get featShellSub => 'Panel, launcher and session controls';

  @override
  String get featLauncher => 'Real app launcher';

  @override
  String get featLauncherSub => 'Reads your installed applications';

  @override
  String get featThemes => 'Themeable';

  @override
  String get featThemesSub => 'Light, dark and OLED wallpapers';

  @override
  String get featUpdates => 'In-app updates';

  @override
  String get featUpdatesSub => 'Checks GitLab releases for you';

  @override
  String get downloads => 'Downloads';

  @override
  String get releasesOnGitLab => 'View releases on GitLab';

  @override
  String get flatpakHint => 'One-line Flatpak install:';

  @override
  String get flatpakRef => 'oasis.flatpakref';

  @override
  String get themeSystem => 'System';

  @override
  String get themeLight => 'Light';

  @override
  String get themeDark => 'Dark';

  @override
  String get themeOled => 'OLED';

  @override
  String get language => 'Language';

  @override
  String get languageSystem => 'System default';

  @override
  String get loadingReleases => 'Loading releases...';

  @override
  String get noReleases => 'No releases yet. Check back soon.';

  @override
  String get releaseNotes => 'Release notes';

  @override
  String get source => 'Source on GitLab';

  @override
  String get pages => 'GitLab Pages';
}
