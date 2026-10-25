import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

import '../l10n/app_localizations.dart';
import '../services/window_actions.dart';

/// macOS-style traffic-light window controls for desktop.
class WindowControls extends StatelessWidget {
  final WindowActions actions;

  const WindowControls({super.key, required this.actions});

  static bool isDesktop(BuildContext context) {
    if (kIsWeb) return false;
    return switch (Theme.of(context).platform) {
      TargetPlatform.linux ||
      TargetPlatform.macOS ||
      TargetPlatform.windows =>
        true,
      _ => false,
    };
  }

  @override
  Widget build(BuildContext context) {
    if (!isDesktop(context)) {
      return const SizedBox.shrink();
    }

    final l = AppLocalizations.of(context);

    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        TrafficLight(
          key: const Key('window_close_button'),
          color: const Color(0xFFFF5F57),
          icon: Icons.close,
          onPressed: actions.close,
          tooltip: l.windowClose,
        ),
        const SizedBox(width: 8),
        TrafficLight(
          key: const Key('window_minimize_button'),
          color: const Color(0xFFFFBD2E),
          icon: Icons.remove,
          onPressed: actions.minimize,
          tooltip: l.windowMinimize,
        ),
        const SizedBox(width: 8),
        TrafficLight(
          key: const Key('window_maximize_button'),
          color: const Color(0xFF28CA41),
          icon: Icons.add,
          onPressed: actions.toggleMaximize,
          tooltip: l.windowZoom,
        ),
      ],
    );
  }
}

class TrafficLight extends StatefulWidget {
  final Color color;
  final IconData icon;
  final VoidCallback onPressed;
  final String tooltip;

  const TrafficLight({
    super.key,
    required this.color,
    required this.icon,
    required this.onPressed,
    required this.tooltip,
  });

  @override
  State<TrafficLight> createState() => _TrafficLightState();
}

class _TrafficLightState extends State<TrafficLight> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    return MouseRegion(
      cursor: SystemMouseCursors.click,
      onEnter: (_) => setState(() => _hovered = true),
      onExit: (_) => setState(() => _hovered = false),
      child: GestureDetector(
        onTap: widget.onPressed,
        child: Tooltip(
          message: widget.tooltip,
          child: SizedBox(
            width: 24,
            height: 24,
            child: Center(
              child: AnimatedContainer(
                duration: const Duration(milliseconds: 100),
                width: _hovered ? 14 : 12,
                height: _hovered ? 14 : 12,
                decoration: BoxDecoration(
                  color: widget.color,
                  shape: BoxShape.circle,
                ),
                child: _hovered
                    ? Icon(
                        widget.icon,
                        size: 9,
                        color: Colors.black.withAlpha(140),
                      )
                    : null,
              ),
            ),
          ),
        ),
      ),
    );
  }
}
