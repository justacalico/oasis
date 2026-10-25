import 'package:flutter/material.dart';

import '../../l10n/app_localizations.dart';
import '../../models/compositor_window.dart';
import '../../models/desktop_entry.dart';
import '../../widgets/app_glyph.dart';

/// The macOS-style dock pinned to the bottom edge of the desktop: a
/// translucent pill holding one magnifying icon per application plus a
/// shortcut to the applications grid.
class OasisDock extends StatelessWidget {
  final List<DesktopEntry> entries;
  final List<CompositorWindow> windows;
  final ValueChanged<DesktopEntry> onLaunch;
  final ValueChanged<CompositorWindow>? onFocusWindow;
  final ValueChanged<CompositorWindow>? onCloseWindow;
  final VoidCallback onShowApps;

  const OasisDock({
    super.key,
    required this.entries,
    required this.onLaunch,
    required this.onShowApps,
    this.windows = const [],
    this.onFocusWindow,
    this.onCloseWindow,
  });

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final scheme = Theme.of(context).colorScheme;

    return Padding(
      padding: const EdgeInsets.only(bottom: 12, left: 16, right: 16),
      child: Container(
        constraints: const BoxConstraints(maxWidth: 720),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
        decoration: BoxDecoration(
          color: scheme.surface.withAlpha(200),
          borderRadius: BorderRadius.circular(28),
          border: Border.all(color: scheme.outlineVariant.withAlpha(120)),
        ),
        child: SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              for (final entry in entries)
                _DockIcon(
                  key: Key('dock_${entry.id}'),
                  tooltip: entry.name,
                  onTap: () => onLaunch(entry),
                  child: AppGlyph(entry: entry, size: 48),
                ),
              for (final w in windows)
                _WindowChip(
                  key: Key('window_${w.id}'),
                  window: w,
                  onTap: () => onFocusWindow?.call(w),
                  onClose: () => onCloseWindow?.call(w),
                ),
              if (entries.isNotEmpty)
                Container(
                  width: 1,
                  height: 40,
                  margin: const EdgeInsets.symmetric(horizontal: 6),
                  color: scheme.outlineVariant,
                ),
              _DockIcon(
                key: const Key('dock_apps_button'),
                tooltip: l.navApps,
                onTap: onShowApps,
                child: Container(
                  width: 48,
                  height: 48,
                  decoration: BoxDecoration(
                    color: scheme.primaryContainer,
                    borderRadius: BorderRadius.circular(13),
                  ),
                  alignment: Alignment.center,
                  child: Icon(
                    Icons.apps,
                    size: 26,
                    color: scheme.onPrimaryContainer,
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _WindowChip extends StatelessWidget {
  final CompositorWindow window;
  final VoidCallback onTap;
  final VoidCallback onClose;

  const _WindowChip({
    super.key,
    required this.window,
    required this.onTap,
    required this.onClose,
  });

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final initial = window.title.isNotEmpty
        ? window.title.characters.first.toUpperCase()
        : '?';
    return _DockIcon(
      tooltip: window.title,
      onTap: onTap,
      onSecondaryTap: onClose,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 48,
            height: 48,
            decoration: BoxDecoration(
              color: scheme.tertiaryContainer,
              borderRadius: BorderRadius.circular(13),
            ),
            alignment: Alignment.center,
            child: Text(
              initial,
              style: TextStyle(
                fontSize: 20,
                fontWeight: FontWeight.w600,
                color: scheme.onTertiaryContainer,
              ),
            ),
          ),
          const SizedBox(height: 2),
          AnimatedContainer(
            duration: const Duration(milliseconds: 120),
            width: window.focused ? 12 : 0,
            height: 3,
            decoration: BoxDecoration(
              color: scheme.primary,
              borderRadius: BorderRadius.circular(2),
            ),
          ),
        ],
      ),
    );
  }
}

class _DockIcon extends StatefulWidget {
  final String tooltip;
  final VoidCallback onTap;
  final VoidCallback? onSecondaryTap;
  final Widget child;

  const _DockIcon({
    super.key,
    required this.tooltip,
    required this.onTap,
    this.onSecondaryTap,
    required this.child,
  });

  @override
  State<_DockIcon> createState() => _DockIconState();
}

class _DockIconState extends State<_DockIcon> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: widget.tooltip,
      child: MouseRegion(
        onEnter: (_) => setState(() => _hovered = true),
        onExit: (_) => setState(() => _hovered = false),
        child: GestureDetector(
          onTap: widget.onTap,
          onSecondaryTap: widget.onSecondaryTap,
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 5),
            child: AnimatedScale(
              scale: _hovered ? 1.25 : 1,
              duration: const Duration(milliseconds: 120),
              child: widget.child,
            ),
          ),
        ),
      ),
    );
  }
}
