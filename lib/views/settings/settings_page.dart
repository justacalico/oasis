import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../l10n/app_localizations.dart';
import '../../state/app_state.dart';
import '../../widgets/wallpaper.dart';

/// Shell preferences: theme, wallpaper and language.
class SettingsPage extends StatelessWidget {
  const SettingsPage({super.key});

  static const _languages = <(String, Locale?)>[
    ('system', null),
    ('Traditional English', Locale('en', 'GB')),
    ('简体中文', Locale.fromSubtags(languageCode: 'zh', scriptCode: 'Hans')),
    ('Русский', Locale('ru')),
    ('Gaeilge', Locale('ga')),
  ];

  String _wallpaperName(AppLocalizations l, Wallpaper w) => switch (w) {
        Wallpaper.dunes => l.wallpaperDunes,
        Wallpaper.lagoon => l.wallpaperLagoon,
        Wallpaper.midnight => l.wallpaperMidnight,
      };

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final state = context.watch<OasisState>();

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        ListTile(
          leading: const Icon(Icons.brightness_6_outlined),
          title: Text(l.theme),
          trailing: SegmentedButton<ThemeMode>(
            segments: [
              ButtonSegment(
                value: ThemeMode.system,
                label: Text(l.themeSystem),
              ),
              ButtonSegment(
                value: ThemeMode.light,
                label: Text(l.themeLight),
              ),
              ButtonSegment(
                value: ThemeMode.dark,
                label: Text(l.themeDark),
              ),
            ],
            selected: {state.themeMode},
            onSelectionChanged: (modes) =>
                state.setThemeMode(modes.first),
          ),
        ),
        ListTile(
          leading: const Icon(Icons.translate),
          title: Text(l.language),
          trailing: DropdownButton<Locale?>(
            key: const Key('language_picker'),
            value: _languages.any((e) => e.$2 == state.locale)
                ? state.locale
                : null,
            items: [
              for (final (label, locale) in _languages)
                DropdownMenuItem(
                  value: locale,
                  child: Text(
                    label == 'system' ? l.languageSystem : label,
                  ),
                ),
            ],
            onChanged: state.setLocale,
          ),
        ),
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 4),
          child: Text(
            l.wallpaper,
            style: Theme.of(context).textTheme.titleSmall,
          ),
        ),
        for (final wallpaper in Wallpaper.values)
          ListTile(
            leading: SizedBox(
              width: 64,
              height: 36,
              child: ClipRRect(
                borderRadius: BorderRadius.circular(8),
                child: WallpaperView(wallpaper: wallpaper),
              ),
            ),
            title: Text(_wallpaperName(l, wallpaper)),
            trailing: wallpaper == state.wallpaper
                ? const Icon(Icons.check)
                : null,
            selected: wallpaper == state.wallpaper,
            onTap: () => state.setWallpaper(wallpaper),
          ),
      ],
    );
  }
}
