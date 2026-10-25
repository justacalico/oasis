import 'package:flutter/material.dart';

abstract final class OasisTheme {
  static const seed = Color(0xFF1F8A70);
  static const fontFamily = 'Inter';

  static ThemeData light() => _base(Brightness.light);

  static ThemeData dark() => _base(Brightness.dark);

  static ThemeData _base(Brightness brightness) {
    final scheme = ColorScheme.fromSeed(
      seedColor: seed,
      brightness: brightness,
    );
    return ThemeData(
      colorScheme: scheme,
      useMaterial3: true,
      fontFamily: fontFamily,
      splashFactory: InkSparkle.splashFactory,
      listTileTheme: const ListTileThemeData(
        shape: StadiumBorder(),
        dense: true,
      ),
    );
  }
}
