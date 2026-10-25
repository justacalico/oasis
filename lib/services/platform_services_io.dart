import 'dart:io';

import '../models/desktop_entry.dart';
import 'battery.dart';
import 'process_runner.dart';

/// Lists .desktop entries from the given applications directories.
class DesktopEntriesLoader {
  final List<Directory> directories;

  const DesktopEntriesLoader(this.directories);

  Future<List<DesktopEntry>> load() async {
    final entries = <String, DesktopEntry>{};
    for (final dir in directories) {
      if (!await dir.exists()) continue;
      await for (final entity in dir.list(followLinks: false)) {
        if (entity is! File || !entity.path.endsWith('.desktop')) continue;
        final id = entity.uri.pathSegments.last.replaceAll('.desktop', '');
        final contents = await entity.readAsString();
        final entry = DesktopEntry.parse(id, contents);
        if (entry != null) entries[id] = entry;
      }
    }
    final list = entries.values.toList()
      ..sort((a, b) => a.name.toLowerCase().compareTo(b.name.toLowerCase()));
    return list;
  }
}

/// Reads the first BAT* device under [powerSupplyPath] (sysfs).
class SysfsBatteryReader {
  final String powerSupplyPath;

  const SysfsBatteryReader({this.powerSupplyPath = '/sys/class/power_supply'});

  Future<BatteryStatus?> read() async {
    final dir = Directory(powerSupplyPath);
    if (!await dir.exists()) return null;
    await for (final entity in dir.list(followLinks: false)) {
      final name =
          entity.path.split(Platform.pathSeparator).last;
      if (entity is! Directory || !name.startsWith('BAT')) continue;
      final capacityFile = File('${entity.path}/capacity');
      final statusFile = File('${entity.path}/status');
      if (!await capacityFile.exists()) continue;
      final capacity = int.tryParse((await capacityFile.readAsString()).trim());
      if (capacity == null) continue;
      final status = await statusFile.exists()
          ? (await statusFile.readAsString()).trim()
          : '';
      return BatteryStatus(capacity: capacity, charging: status == 'Charging');
    }
    return null;
  }
}

ProcessRunner processRunner() =>
    (command, args) => Process.run(command, args)
        .then((result) => result.exitCode)
        .catchError((_) => -1);

DesktopEntriesLoader defaultEntriesLoader() => DesktopEntriesLoader([
      Directory('/usr/share/applications'),
      Directory(
        '${Platform.environment['HOME'] ?? ''}/.local/share/applications',
      ),
    ]);

SysfsBatteryReader defaultBatteryReader() => const SysfsBatteryReader();
