import 'package:flutter/material.dart';

import '../models/desktop_entry.dart';

/// A deterministic icon tile for a desktop entry: a tinted rounded square
/// with the application's initial. Real icon-theme lookup needs a compositor
/// side channel, so the shell renders its own consistent glyphs.
class AppGlyph extends StatelessWidget {
  final DesktopEntry entry;
  final double size;

  const AppGlyph({super.key, required this.entry, this.size = 56});

  static const _palette = [
    Color(0xFF1F8A70),
    Color(0xFF3F6FB5),
    Color(0xFFB5533F),
    Color(0xFF8A5FBF),
    Color(0xFFB58A3F),
    Color(0xFF3F9EB5),
    Color(0xFF7BB53F),
    Color(0xFFB53F7B),
  ];

  Color _color() {
    var hash = 0;
    for (final unit in entry.id.codeUnits) {
      hash = (hash * 31 + unit) & 0x7fffffff;
    }
    return _palette[hash % _palette.length];
  }

  String get _initial => entry.name.isEmpty ? '?' : entry.name[0].toUpperCase();

  @override
  Widget build(BuildContext context) {
    final color = _color();
    return Container(
      width: size,
      height: size,
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(size * 0.28),
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [color, Color.lerp(color, Colors.black, 0.25)!],
        ),
      ),
      alignment: Alignment.center,
      child: Text(
        _initial,
        style: TextStyle(
          color: Colors.white,
          fontSize: size * 0.44,
          fontWeight: FontWeight.w600,
        ),
      ),
    );
  }
}
