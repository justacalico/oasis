import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../l10n/app_localizations.dart';
import '../../models/desktop_entry.dart';
import '../../services/app_launcher.dart';
import '../../services/platform_services.dart';
import '../../state/app_state.dart';
import '../../widgets/app_glyph.dart';
import '../desktop/desktop_page.dart';

/// The applications grid: real .desktop entries on Linux, search-filtered.
class AppsPage extends StatefulWidget {
  final DesktopEntriesLoader loader;
  final AppLauncher launcher;

  const AppsPage({super.key, required this.loader, required this.launcher});

  @override
  State<AppsPage> createState() => _AppsPageState();
}

class _AppsPageState extends State<AppsPage> {
  List<DesktopEntry>? _entries;

  @override
  void initState() {
    super.initState();
    _reload();
  }

  Future<void> _reload() async {
    final entries = await widget.loader.load();
    if (mounted) setState(() => _entries = entries);
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final query = context
        .select<OasisState, String>((s) => s.launcherQuery)
        .toLowerCase();
    final state = context.read<OasisState>();

    final entries = _entries;
    final filtered = entries
        ?.where(
            (e) => query.isEmpty || e.name.toLowerCase().contains(query))
        .toList();

    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
          child: Row(
            children: [
              Expanded(
                child: TextField(
                  key: const Key('app_search'),
                  decoration: InputDecoration(
                    hintText: l.searchApps,
                    prefixIcon: const Icon(Icons.search),
                    isDense: true,
                    border: const OutlineInputBorder(
                      borderRadius: BorderRadius.all(Radius.circular(24)),
                    ),
                  ),
                  onChanged: state.setLauncherQuery,
                ),
              ),
              const SizedBox(width: 8),
              IconButton(
                key: const Key('apps_refresh'),
                tooltip: l.refresh,
                icon: const Icon(Icons.refresh),
                onPressed: _reload,
              ),
            ],
          ),
        ),
        Expanded(
          child: switch (filtered) {
            null => const Center(child: CircularProgressIndicator()),
            [] => Center(child: Text(l.noApps)),
            final list => GridView.builder(
                padding: const EdgeInsets.all(16),
                gridDelegate:
                    const SliverGridDelegateWithMaxCrossAxisExtent(
                  maxCrossAxisExtent: 120,
                  mainAxisSpacing: 12,
                  crossAxisSpacing: 12,
                  childAspectRatio: 0.85,
                ),
                itemCount: list.length,
                itemBuilder: (context, i) => _AppTile(
                  entry: list[i],
                  onTap: () => launchEntry(context, widget.launcher, list[i]),
                ),
              ),
          },
        ),
      ],
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
          AppGlyph(entry: entry),
          const SizedBox(height: 8),
          Text(
            entry.name,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            textAlign: TextAlign.center,
            style: Theme.of(context).textTheme.bodySmall,
          ),
        ],
      ),
    );
  }
}
