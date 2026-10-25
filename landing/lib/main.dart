import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';
import 'package:url_launcher/url_launcher.dart';

import 'l10n/landing_l10n.dart';
import 'showcase.dart';

const projectUrl = 'https://gitlab.com/HttpAnimations/oasis';
const releasesUrl = '$projectUrl/-/releases';
const releasesApi =
    'https://gitlab.com/api/v4/projects/HttpAnimations%2Foasis/releases?per_page=20';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  final prefs = await SharedPreferences.getInstance();
  runApp(LandingApp(prefs: prefs));
}

class LandingApp extends StatefulWidget {
  final SharedPreferences prefs;
  const LandingApp({super.key, required this.prefs});

  @override
  State<LandingApp> createState() => _LandingAppState();
}

enum LandingTheme { system, light, dark, oled }

class _LandingAppState extends State<LandingApp> {
  late LandingTheme _theme;
  Locale? _locale;

  @override
  void initState() {
    super.initState();
    _theme = LandingTheme.values[widget.prefs.getInt('theme') ?? 0];
    final code = widget.prefs.getString('locale');
    if (code != null) _locale = Locale(code);
  }

  ThemeData _themeData() => switch (_theme) {
        LandingTheme.light => _base(Brightness.light, Colors.white),
        LandingTheme.dark ||
        LandingTheme.oled =>
          _base(Brightness.dark, _theme == LandingTheme.oled
              ? Colors.black
              : const Color(0xFF0E1626)),
        LandingTheme.system =>
          MediaQuery.platformBrightnessOf(context) == Brightness.dark
              ? _base(Brightness.dark, const Color(0xFF0E1626))
              : _base(Brightness.light, Colors.white),
      };

  ThemeData _base(Brightness b, Color surface) {
    final scheme = ColorScheme.fromSeed(
      seedColor: const Color(0xFF1F8A70),
      brightness: b,
    ).copyWith(surface: surface, surfaceContainerLow: surface);
    return ThemeData(colorScheme: scheme, useMaterial3: true);
  }

  void _setTheme(LandingTheme t) {
    setState(() => _theme = t);
    widget.prefs.setInt('theme', t.index);
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Oasis',
      debugShowCheckedModeBanner: false,
      theme: _themeData(),
      locale: _locale,
      supportedLocales: LandingL10n.supportedLocales,
      localizationsDelegates: const [
        LandingL10n.delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
      ],
      home: LandingHome(
        theme: _theme,
        onTheme: _setTheme,
        locale: _locale,
        onLocale: (l) {
          setState(() => _locale = l);
          if (l == null) {
            widget.prefs.remove('locale');
          } else {
            widget.prefs.setString('locale', l.languageCode);
          }
        },
      ),
    );
  }
}

class LandingHome extends StatefulWidget {
  final LandingTheme theme;
  final ValueChanged<LandingTheme> onTheme;
  final Locale? locale;
  final ValueChanged<Locale?> onLocale;

  const LandingHome({
    super.key,
    required this.theme,
    required this.onTheme,
    required this.locale,
    required this.onLocale,
  });

  @override
  State<LandingHome> createState() => _LandingHomeState();
}

class _LandingHomeState extends State<LandingHome> {
  List<Map<String, dynamic>>? _releases;

  @override
  void initState() {
    super.initState();
    http
        .get(Uri.parse(releasesApi))
        .then((r) => jsonDecode(r.body))
        .then((data) {
      if (mounted) {
        setState(() => _releases = (data as List).cast<Map<String, dynamic>>());
      }
    }).catchError((_) {
      if (mounted) setState(() => _releases = []);
    });
  }

  @override
  Widget build(BuildContext context) {
    final l = LandingL10n.of(context);
    final scheme = Theme.of(context).colorScheme;
    final width = MediaQuery.sizeOf(context).width;
    final wide = width > 860;

    return Scaffold(
      appBar: AppBar(
        title: Text(l.title),
        actions: [
          DropdownButton<Locale?>(
            value: _localeOf(widget.locale),
            underline: const SizedBox(),
            items: [
              DropdownMenuItem(value: null, child: Text(l.languageSystem)),
              const DropdownMenuItem(
                  value: Locale('en', 'GB'), child: Text('Traditional English')),
              const DropdownMenuItem(
                  value: Locale('zh'), child: Text('简体中文')),
              const DropdownMenuItem(
                  value: Locale('ru'), child: Text('Русский')),
              const DropdownMenuItem(
                  value: Locale('ga'), child: Text('Gaeilge')),
            ],
            onChanged: widget.onLocale,
          ),
          const SizedBox(width: 8),
          SegmentedButton<LandingTheme>(
            segments: [
              ButtonSegment(
                  value: LandingTheme.system, label: Text(l.themeSystem)),
              ButtonSegment(
                  value: LandingTheme.light, label: Text(l.themeLight)),
              ButtonSegment(
                  value: LandingTheme.dark, label: Text(l.themeDark)),
              ButtonSegment(
                  value: LandingTheme.oled, label: Text(l.themeOled)),
            ],
            selected: {widget.theme},
            onSelectionChanged: (s) => widget.onTheme(s.first),
          ),
          const SizedBox(width: 16),
        ],
      ),
      body: SingleChildScrollView(
        child: Column(
          children: [
            Padding(
              padding: EdgeInsets.symmetric(
                  horizontal: wide ? 80 : 24, vertical: 48),
              child: Flex(
                direction: wide ? Axis.horizontal : Axis.vertical,
                crossAxisAlignment: CrossAxisAlignment.center,
                children: [
                  Expanded(
                    flex: wide ? 1 : 0,
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(l.title,
                            style: Theme.of(context)
                                .textTheme
                                .displayLarge
                                ?.copyWith(fontWeight: FontWeight.w800)),
                        Text(l.tagline,
                            style:
                                Theme.of(context).textTheme.headlineSmall),
                        const SizedBox(height: 16),
                        Text(l.heroText,
                            style: Theme.of(context).textTheme.bodyLarge),
                        const SizedBox(height: 24),
                        Wrap(
                          spacing: 12,
                          runSpacing: 12,
                          children: [
                            FilledButton.icon(
                              icon: const Icon(Icons.download),
                              label: Text(l.download),
                              onPressed: () =>
                                  launchUrl(Uri.parse(releasesUrl)),
                            ),
                            OutlinedButton.icon(
                              icon: const Icon(Icons.open_in_new),
                              label: Text(l.tryWeb),
                              onPressed: () =>
                                  launchUrl(Uri.parse('app/')),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                  const SizedBox(width: 48, height: 32),
                  SizedBox(
                    width: wide ? 480 : width - 48,
                    child: const ShowcaseCard(),
                  ),
                ],
              ),
            ),
            _features(l, scheme, wide),
            _downloadSection(l, scheme, wide),
            _footer(l, scheme),
          ],
        ),
      ),
    );
  }

  Locale? _localeOf(Locale? l) => l;

  Widget _features(LandingL10n l, ColorScheme s, bool wide) {
    final feats = [
      (Icons.desktop_windows, l.featShell, l.featShellSub),
      (Icons.apps, l.featLauncher, l.featLauncherSub),
      (Icons.palette_outlined, l.featThemes, l.featThemesSub),
      (Icons.system_update_alt, l.featUpdates, l.featUpdatesSub),
    ];
    return Padding(
      padding: EdgeInsets.symmetric(horizontal: wide ? 80 : 24, vertical: 24),
      child: GridView.count(
        crossAxisCount: wide ? 4 : 2,
        shrinkWrap: true,
        physics: const NeverScrollableScrollPhysics(),
        childAspectRatio: 1.4,
        children: [
          for (final (icon, title, sub) in feats)
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Icon(icon, color: s.primary),
                    const Spacer(),
                    Text(title,
                        style: Theme.of(context).textTheme.titleSmall),
                    Text(sub, style: Theme.of(context).textTheme.bodySmall),
                  ],
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _downloadSection(LandingL10n l, ColorScheme s, bool wide) {
    return Container(
      width: double.infinity,
      color: s.surfaceContainerHighest.withAlpha(120),
      padding: EdgeInsets.symmetric(horizontal: wide ? 80 : 24, vertical: 40),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Text(l.downloads,
                  style: Theme.of(context).textTheme.headlineSmall),
              const Spacer(),
              OutlinedButton.icon(
                icon: const Icon(Icons.new_releases_outlined),
                label: Text(l.releasesOnGitLab),
                onPressed: () => launchUrl(Uri.parse(releasesUrl)),
              ),
            ],
          ),
          const SizedBox(height: 16),
          SelectableText('${l.flatpakHint}\n'
              'flatpak install https://httpanimations.gitlab.io/oasis/oasis.flatpakref'),
          const SizedBox(height: 16),
          if (_releases == null)
            Text(l.loadingReleases)
          else if (_releases!.isEmpty)
            Text(l.noReleases)
          else
            for (final r in _releases!)
              Card(
                child: ListTile(
                  leading: const Icon(Icons.tag),
                  title: Text(r['tag_name'] as String? ?? ''),
                  subtitle: Text((r['name'] as String?) ?? ''),
                  trailing: const Icon(Icons.open_in_new, size: 18),
                  onTap: () => launchUrl(Uri.parse(
                      (r['_links']?['self'] as String?) ??
                          '$releasesUrl/${r['tag_name']}')),
                ),
              ),
        ],
      ),
    );
  }

  Widget _footer(LandingL10n l, ColorScheme s) => Padding(
        padding: const EdgeInsets.all(24),
        child: Wrap(
          spacing: 24,
          children: [
            TextButton(
              onPressed: () => launchUrl(Uri.parse(projectUrl)),
              child: Text(l.source),
            ),
            TextButton(
              onPressed: () => launchUrl(
                  Uri.parse('https://httpanimations.gitlab.io/oasis/')),
              child: Text(l.pages),
            ),
          ],
        ),
      );
}
