import 'dart:ui' show ImageFilter;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';

import '../../l10n/app_localizations.dart';
import '../../models/desktop_entry.dart';
import '../../services/app_launcher.dart';
import '../../services/platform_services.dart';
import '../../state/app_state.dart';
import '../../widgets/app_glyph.dart';
import '../desktop/desktop_page.dart';

/// The Launchpad-style applications overlay: the current screen dimmed and
/// blurred behind a search field and a grid of every installed .desktop
/// entry. Closes on Escape, on a tap anywhere outside a control, or when an
/// app launches.
class AppsOverlay extends StatefulWidget {
  final DesktopEntriesLoader loader;
  final AppLauncher launcher;

  const AppsOverlay({super.key, required this.loader, required this.launcher});

  @override
  State<AppsOverlay> createState() => _AppsOverlayState();
}

class _AppsOverlayState extends State<AppsOverlay> {
  List<DesktopEntry>? _entries;

  @override
  void initState() {
    super.initState();
    _load(widget.loader.load);
  }

  Future<void> _load(Future<List<DesktopEntry>> Function() source) async {
    final entries = await source();
    if (mounted) setState(() => _entries = entries);
  }

  Future<void> _launch(DesktopEntry entry) async {
    await launchEntry(context, widget.launcher, entry);
    if (mounted) context.read<OasisState>().closeLauncher();
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final query = context
        .select<OasisState, String>((s) => s.launcherQuery)
        .toLowerCase();
    final state = context.read<OasisState>();
    final scheme = Theme.of(context).colorScheme;

    final entries = _entries;
    final filtered = entries
        ?.where(
            (e) => query.isEmpty || e.name.toLowerCase().contains(query))
        .toList();

    return Focus(
      autofocus: true,
      onKeyEvent: (node, event) {
        if (event is KeyDownEvent &&
            event.logicalKey == LogicalKeyboardKey.escape) {
          state.closeLauncher();
          return KeyEventResult.handled;
        }
        return KeyEventResult.ignored;
      },
      child: TweenAnimationBuilder<double>(
        tween: Tween(begin: 0, end: 1),
        duration: const Duration(milliseconds: 180),
        curve: Curves.easeOut,
        builder: (context, t, child) => Opacity(
          opacity: t,
          child: Transform.scale(scale: 1.06 - 0.06 * t, child: child),
        ),
        child: GestureDetector(
          behavior: HitTestBehavior.translucent,
          onTap: state.closeLauncher,
          child: Stack(
            fit: StackFit.expand,
            children: [
              Positioned.fill(
                child: BackdropFilter(
                  filter: ImageFilter.blur(sigmaX: 28, sigmaY: 28),
                  child:
                      const ColoredBox(color: Color(0x73000000)),
                ),
              ),
              SafeArea(
                child: Column(
                  children: [
                    Padding(
                      padding:
                          const EdgeInsets.fromLTRB(24, 48, 24, 24),
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          ConstrainedBox(
                            constraints:
                                const BoxConstraints(maxWidth: 360),
                            child: TextField(
                              key: const Key('app_search'),
                              autofocus: true,
                              decoration: InputDecoration(
                                hintText: l.searchApps,
                                prefixIcon:
                                    const Icon(Icons.search),
                                isDense: true,
                                filled: true,
                                fillColor:
                                    scheme.surface.withAlpha(170),
                                border: const OutlineInputBorder(
                                  borderRadius: BorderRadius.all(
                                      Radius.circular(24)),
                                ),
                              ),
                              onChanged: state.setLauncherQuery,
                            ),
                          ),
                          IconButton(
                            key: const Key('apps_refresh'),
                            tooltip: l.refresh,
                            color: Colors.white,
                            icon: const Icon(Icons.refresh),
                            onPressed: () =>
                                _load(widget.loader.reload),
                          ),
                        ],
                      ),
                    ),
                    Expanded(
                      child: switch (filtered) {
                        null => const Center(
                            child: CircularProgressIndicator()),
                        [] => Center(
                            child: Text(
                              l.noApps,
                              style: const TextStyle(
                                  color: Colors.white70),
                            ),
                          ),
                        final list => GridView.builder(
                            padding: const EdgeInsets.fromLTRB(
                                48, 8, 48, 48),
                            gridDelegate:
                                const SliverGridDelegateWithMaxCrossAxisExtent(
                              maxCrossAxisExtent: 148,
                              mainAxisSpacing: 24,
                              crossAxisSpacing: 24,
                              childAspectRatio: 0.82,
                            ),
                            itemCount: list.length,
                            itemBuilder: (context, i) => _AppTile(
                              entry: list[i],
                              onTap: () => _launch(list[i]),
                            ),
                          ),
                      },
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _AppTile extends StatelessWidget {
  final DesktopEntry entry;
  final VoidCallback onTap;

  const _AppTile({required this.entry, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return InkWell(
      borderRadius: BorderRadius.circular(16),
      onTap: onTap,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          AppGlyph(entry: entry, size: 64),
          const SizedBox(height: 10),
          Text(
            entry.name,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            textAlign: TextAlign.center,
            style: const TextStyle(
              color: Colors.white,
              fontSize: 13,
              shadows: [
                Shadow(blurRadius: 4, color: Colors.black54),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
