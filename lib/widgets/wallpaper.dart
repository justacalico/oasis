import 'package:flutter/material.dart';

import '../state/app_state.dart';

/// Paints the Oasis wallpapers procedurally so the shell ships no image
/// assets.
class WallpaperPainter extends CustomPainter {
  final Wallpaper wallpaper;

  const WallpaperPainter(this.wallpaper);

  @override
  void paint(Canvas canvas, Size size) {
    switch (wallpaper) {
      case Wallpaper.dunes:
        _dunes(canvas, size);
      case Wallpaper.lagoon:
        _lagoon(canvas, size);
      case Wallpaper.midnight:
        _midnight(canvas, size);
    }
  }

  void _sky(Canvas canvas, Size size, List<Color> colors) {
    final paint = Paint()
      ..shader = LinearGradient(
        begin: Alignment.topCenter,
        end: Alignment.bottomCenter,
        colors: colors,
      ).createShader(Offset.zero & size);
    canvas.drawRect(Offset.zero & size, paint);
  }

  void _dune(Canvas canvas, Size size, double top, Color color,
      {double amplitude = 40, double phase = 0}) {
    final path = Path()..moveTo(0, size.height);
    for (var x = 0.0; x <= size.width; x += 8) {
      final y = top +
          amplitude *
              _wave(x / size.width * 3.14159 * 2 + phase);
      path.lineTo(x, y);
    }
    path
      ..lineTo(size.width, size.height)
      ..close();
    canvas.drawPath(path, Paint()..color = color);
  }

  double _wave(double t) => (1 - (t % (2 * 3.14159)).abs() / 3.14159) * 2 - 1;

  void _dunes(Canvas canvas, Size size) {
    _sky(canvas, size, const [
      Color(0xFF2E7D6B),
      Color(0xFF7FC8A9),
      Color(0xFFF7E7B4),
    ]);
    canvas.drawCircle(
      Offset(size.width * 0.72, size.height * 0.24),
      size.shortestSide * 0.09,
      Paint()..color = const Color(0xFFFFF3C4),
    );
    _dune(canvas, size, size.height * 0.62, const Color(0xFFE8C880),
        amplitude: 46);
    _dune(canvas, size, size.height * 0.74, const Color(0xFFC89B5A),
        amplitude: 38, phase: 2.1);
    _dune(canvas, size, size.height * 0.88, const Color(0xFF8F6B3D),
        amplitude: 30, phase: 4.2);
  }

  void _lagoon(Canvas canvas, Size size) {
    _sky(canvas, size, const [
      Color(0xFF0B3D5C),
      Color(0xFF1F8A70),
      Color(0xFF3FB8AF),
    ]);
    canvas.drawOval(
      Rect.fromCenter(
        center: Offset(size.width * 0.5, size.height * 0.68),
        width: size.width * 0.66,
        height: size.height * 0.34,
      ),
      Paint()..color = const Color(0xFF7FD8C9),
    );
    canvas.drawOval(
      Rect.fromCenter(
        center: Offset(size.width * 0.5, size.height * 0.66),
        width: size.width * 0.5,
        height: size.height * 0.24,
      ),
      Paint()..color = const Color(0xFFAFE8DE),
    );
    _dune(canvas, size, size.height * 0.9, const Color(0xFF0B3D5C),
        amplitude: 26, phase: 1.4);
  }

  void _midnight(Canvas canvas, Size size) {
    _sky(canvas, size, const [
      Color(0xFF050810),
      Color(0xFF0E1B33),
      Color(0xFF1B2E52),
    ]);
    canvas.drawCircle(
      Offset(size.width * 0.24, size.height * 0.22),
      size.shortestSide * 0.07,
      Paint()..color = const Color(0xFFE8ECF5),
    );
    canvas.drawCircle(
      Offset(size.width * 0.22, size.height * 0.20),
      size.shortestSide * 0.06,
      Paint()..color = const Color(0xFF0E1B33),
    );
    final starPaint = Paint()..color = const Color(0xCCFFFFFF);
    for (var i = 0; i < 40; i++) {
      final dx = (i * 97.0 % size.width);
      final dy = (i * 57.0 % (size.height * 0.55));
      canvas.drawCircle(Offset(dx, dy), i % 3 == 0 ? 1.6 : 1.0, starPaint);
    }
    _dune(canvas, size, size.height * 0.8, const Color(0xFF101A2C),
        amplitude: 34, phase: 0.8);
  }

  @override
  bool shouldRepaint(WallpaperPainter oldDelegate) =>
      oldDelegate.wallpaper != wallpaper;
}

/// Fills its parent with the painted wallpaper.
class WallpaperView extends StatelessWidget {
  final Wallpaper wallpaper;

  const WallpaperView({super.key, required this.wallpaper});

  @override
  Widget build(BuildContext context) => CustomPaint(
        painter: WallpaperPainter(wallpaper),
        size: Size.infinite,
      );
}
