import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/app_localizations.dart';
import '../state/app_state.dart';

enum AppDestination {
  desktop(Icons.desktop_windows_outlined),
  apps(Icons.apps),
  settings(Icons.settings_outlined),
  about(Icons.info_outline);

  final IconData icon;
  const AppDestination(this.icon);

  String label(AppLocalizations l) => switch (this) {
        AppDestination.desktop => l.navDesktop,
        AppDestination.apps => l.navApps,
        AppDestination.settings => l.navSettings,
        AppDestination.about => l.navAbout,
      };
}

/// The wide row-list navigation sidebar: one horizontal row per destination
/// with a pill highlight on the selected row.
class OasisSidebar extends StatelessWidget {
  final VoidCallback? onNavigate;

  const OasisSidebar({super.key, this.onNavigate});

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final selected =
        context.select<OasisState, int>((s) => s.destinationIndex);
    final state = context.read<OasisState>();

    return Container(
      width: 240,
      color: Theme.of(context).colorScheme.surfaceContainerLow,
      child: SafeArea(
        child: ListView(
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
          children: [
            for (var i = 0; i < AppDestination.values.length; i++)
              Material(
                color: Colors.transparent,
                shape: const StadiumBorder(),
                clipBehavior: Clip.antiAlias,
                child: ListTile(
                  leading: Icon(AppDestination.values[i].icon, size: 20),
                  title: Text(
                    AppDestination.values[i].label(l),
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                  ),
                  dense: true,
                  selected: i == selected,
                  selectedTileColor:
                      Theme.of(context).colorScheme.secondaryContainer,
                  onTap: () {
                    state.setDestinationIndex(i);
                    onNavigate?.call();
                  },
                ),
              ),
          ],
        ),
      ),
    );
  }
}
