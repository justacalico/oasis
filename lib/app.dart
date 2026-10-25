import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'l10n/app_localizations.dart';
import 'state/app_state.dart';
import 'theme/app_theme.dart';
import 'views/shell.dart';

/// The Oasis app root.
class OasisApp extends StatelessWidget {
  final SharedPreferences prefs;
  final ShellServices? services;
  final OasisState? stateOverride;
  final TargetPlatform? platform;

  const OasisApp({
    super.key,
    required this.prefs,
    this.services,
    this.stateOverride,
    this.platform,
  });

  @override
  Widget build(BuildContext context) {
    final state = stateOverride ?? OasisState(prefs);
    return ChangeNotifierProvider<OasisState>.value(
      value: state,
      child: Consumer<OasisState>(
        builder: (context, s, _) => MaterialApp(
          onGenerateTitle: (context) => AppLocalizations.of(context).appTitle,
          debugShowCheckedModeBanner: false,
          theme: platform == null
              ? OasisTheme.light()
              : OasisTheme.light().copyWith(platform: platform),
          darkTheme: platform == null
              ? OasisTheme.dark()
              : OasisTheme.dark().copyWith(platform: platform),
          themeMode: s.themeMode,
          locale: s.locale,
          supportedLocales: AppLocalizations.supportedLocales,
          localizationsDelegates: const [
            AppLocalizations.delegate,
            GlobalMaterialLocalizations.delegate,
            GlobalWidgetsLocalizations.delegate,
            GlobalCupertinoLocalizations.delegate,
          ],
          home: OasisShell(services: services ?? ShellServices.platform()),
        ),
      ),
    );
  }
}
