import 'package:flutter/material.dart';

/// A custom-painted preview of the Oasis desktop for the landing page hero.
class ShowcasePainter extends CustomPainter {
  final ColorScheme scheme;

  ShowcasePainter(this.scheme);

  @override
  void paint(Canvas canvas, Size size) {
    final sky = Paint()
      ..shader = LinearGradient(
        begin: Alignment.topCenter,
        end: Alignment.bottomCenter,
        colors: const [Color(0xFF0B3D5C), Color(0xFF3FB8AF)],
      ).createShader(Offset.zero & size);
    final r = Offset.zero & size;
    canvas.drawRRect(
        RRect.fromRectAndRadius(r, const Radius.circular(16)), sky);

    canvas.drawCircle(
        Offset(size.width * 0.76, size.height * 0.2),
        size.shortestSide * 0.08,
        Paint()..color = const Color(0xFFFFF3C4));

    void dune(double top, Color c) {
      final path = Path()..moveTo(0, size.height);
      for (var x = 0.0; x <= size.width; x += 6) {
        path.lineTo(
            x,
            top +
                18 *
                    (1 -
                        ((x / size.width * 6.28).abs() % 6.28 / 3.14) * 2 +
                        1));
      }
      path
        ..lineTo(size.width, size.height)
        ..close();
      canvas.drawPath(path, Paint()..color = c);
    }

    dune(size.height * 0.68, const Color(0xFFE8C880));
    dune(size.height * 0.8, const Color(0xFF8F6B3D));

    final panel = Paint()..color = scheme.surface.withAlpha(230);
    canvas.drawRRect(
        RRect.fromRectAndRadius(
            Rect.fromLTWH(0, 0, size.width, 26), const Radius.circular(0)),
        panel);
    canvas.drawRect(Rect.fromLTWH(0, 26, size.width, 1),
        Paint()..color = scheme.outline.withAlpha(80));

    final dot = Paint()..color = scheme.primary;
    for (var i = 0; i < 3; i++) {
      canvas.drawCircle(
          Offset(size.width - 14.0 - i * 14, 13), 4, dot);
    }

    final tilePaint = Paint()..color = scheme.surface.withAlpha(180);
    final icons = [Icons.apps, Icons.folder, Icons.terminal, Icons.public];
    for (var i = 0; i < icons.length; i++) {
      final x = 24.0 + i * 56;
      canvas.drawRRect(
          RRect.fromRectAndRadius(
              Rect.fromLTWH(x, size.height - 60, 44, 44),
              const Radius.circular(12)),
          tilePaint);
      final tp = TextPainter(
        text: TextSpan(
          text: String.fromCharCode(icons[i].codePoint),
          style: TextStyle(
            fontFamily: icons[i].fontFamily,
            fontSize: 22,
            color: scheme.primary,
          ),
        ),
        textDirection: TextDirection.ltr,
      )..layout();
      tp.paint(canvas, Offset(x + 11, size.height - 49));
    }
  }

  @override
  bool shouldRepaint(ShowcasePainter old) => old.scheme != scheme;
}

class ShowcaseCard extends StatelessWidget {
  const ShowcaseCard({super.key});

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return AspectRatio(
      aspectRatio: 16 / 10,
      child: CustomPaint(painter: ShowcasePainter(scheme)),
    );
  }
}
