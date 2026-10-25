import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:provider/provider.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../l10n/app_localizations.dart';
import '../../models/app_update.dart';
import '../../services/version_checker.dart';
import '../../state/app_state.dart';

/// About page: version, update check and project links.
class AboutPage extends StatefulWidget {
  final Future<PackageInfo> Function() packageInfo;
  final Future<bool> Function(Uri) openUrl;

  const AboutPage({
    super.key,
    this.packageInfo = PackageInfo.fromPlatform,
    this.openUrl = launchUrl,
  });

  @override
  State<AboutPage> createState() => _AboutPageState();
}

class _AboutPageState extends State<AboutPage> {
  PackageInfo? _info;
  bool _checkFailed = false;

  @override
  void initState() {
    super.initState();
    widget.packageInfo().then((info) {
      if (mounted) setState(() => _info = info);
    });
  }

  Future<void> _check() async {
    final state = context.read<OasisState>();
    final version = _info?.version ?? '';
    final update = await state.checkForAppUpdate(version);
    if (mounted && update == null) {
      setState(() => _checkFailed = true);
    } else if (mounted) {
      setState(() => _checkFailed = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final state = context.watch<OasisState>();
    final update = state.appUpdate;

    return ListView(
      padding: const EdgeInsets.all(24),
      children: [
        Center(
          child: ClipRRect(
            borderRadius: BorderRadius.circular(20),
            child: Image.asset('assets/icon.png', width: 96, height: 96),
          ),
        ),
        const SizedBox(height: 12),
        Center(
          child: Text(
            l.appTitle,
            style: Theme.of(context).textTheme.headlineSmall,
          ),
        ),
        Center(child: Text(l.aboutTagline)),
        const SizedBox(height: 24),
        ListTile(
          leading: const Icon(Icons.tag),
          title: Text(l.version),
          trailing: Text(_info?.version ?? '—'),
        ),
        const Divider(),
        UpdateTile(
          checking: state.checkingUpdate,
          failed: _checkFailed,
          update: update,
          onCheck: _check,
          onOpenUrl: widget.openUrl,
        ),
        const Divider(),
        ListTile(
          leading: const Icon(Icons.new_releases_outlined),
          title: Text(l.allReleases),
          trailing: const Icon(Icons.open_in_new, size: 18),
          onTap: () =>
              widget.openUrl(Uri.parse(VersionChecker.releasesUrl)),
        ),
        ListTile(
          leading: const Icon(Icons.code),
          title: Text(l.projectPage),
          trailing: const Icon(Icons.open_in_new, size: 18),
          onTap: () => widget.openUrl(
            Uri.parse('https://gitlab.com/HttpAnimations/oasis'),
          ),
        ),
      ],
    );
  }
}

class UpdateTile extends StatelessWidget {
  final bool checking;
  final bool failed;
  final AppUpdate? update;
  final VoidCallback onCheck;
  final Future<bool> Function(Uri) onOpenUrl;

  const UpdateTile({
    super.key,
    required this.checking,
    required this.failed,
    required this.update,
    required this.onCheck,
    required this.onOpenUrl,
  });

  @override
  Widget build(BuildContext context) {
    final l = AppLocalizations.of(context);
    final update = this.update;

    Widget subtitle;
    if (checking) {
      subtitle = Text(l.checking);
    } else if (failed) {
      subtitle = Text(l.checkFailed);
    } else if (update == null) {
      subtitle = Text(l.checkUpdates);
    } else if (update.updateAvailable) {
      subtitle = Text(l.updateAvailable(update.latestVersion));
    } else {
      subtitle = Text(l.upToDate);
    }

    return Column(
      children: [
        ListTile(
          key: const Key('check_updates_tile'),
          leading: checking
              ? const SizedBox(
                  width: 20,
                  height: 20,
                  child: CircularProgressIndicator(strokeWidth: 2),
                )
              : const Icon(Icons.system_update_alt),
          title: Text(l.checkUpdates),
          subtitle: subtitle,
          onTap: checking ? null : onCheck,
        ),
        if (update != null && update.updateAvailable)
          ListTile(
            key: const Key('view_release_tile'),
            leading: const Icon(Icons.open_in_new),
            title: Text(l.viewRelease),
            onTap: () => onOpenUrl(Uri.parse(update.releaseUrl)),
          ),
      ],
    );
  }
}
