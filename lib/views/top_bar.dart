import 'package:flutter/material.dart';

import '../services/window_actions.dart';
import 'window_controls.dart';

/// The macOS-style top bar every desktop screen shares: traffic lights on the
/// left, centred window title, and the rest is a drag region that
/// double-clicks to maximise/restore.
class TopBar extends StatelessWidget implements PreferredSizeWidget {
  final String title;
  final WindowActions actions;
  final Widget? leading;
  final List<Widget> trailing;

  const TopBar({
    super.key,
    required this.title,
    required this.actions,
    this.leading,
    this.trailing = const [],
  });

  @override
  Size get preferredSize => const Size.fromHeight(38);

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Material(
      color: scheme.surfaceContainerLow,
      child: GestureDetector(
        onPanStart: (_) => actions.startDragging(),
        onDoubleTap: actions.toggleMaximize,
        behavior: HitTestBehavior.opaque,
        child: SizedBox(
          height: preferredSize.height,
          child: Row(
            children: [
              const SizedBox(width: 12),
              WindowControls(actions: actions),
              if (leading != null) ...[
                const SizedBox(width: 8),
                leading!,
              ],
              Expanded(
                child: Center(
                  child: Text(
                    title,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: Theme.of(context).textTheme.titleSmall,
                  ),
                ),
              ),
              ...trailing,
              const SizedBox(width: 12),
            ],
          ),
        ),
      ),
    );
  }
}
