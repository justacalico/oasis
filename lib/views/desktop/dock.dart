import 'package:flutter/material.dart';

import '../../l10n/app_localizations.dart';
import '../../models/desktop_entry.dart';
import '../../widgets/app_glyph.dart';

/// The macOS-style dock pinned to the bottom edge of the desktop: a
/// translucent pill holding one magnifying icon per application plus a
/// shortcut to the applications grid.
class OasisDock extends StatelessWidget {
  final List<DesktopEntry> entries;
  final ValueChanged<DesktopEntry> onLaunch;
  final VoidCallback onShowApps;

  const OasisDock({
    super.key,
    required this.entries,
    required this.onLaunch,
    required this.onShowApps,
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

class _DockIcon extends StatefulWidget {
  final String tooltip;
  final VoidCallback onTap;
  final Widget child;

  const _DockIcon({
    super.key,
    required this.tooltip,
    required this.onTap,
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
