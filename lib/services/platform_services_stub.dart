import '../models/desktop_entry.dart';
import 'battery.dart';
import 'process_runner.dart';

/// Fallback entries shown where .desktop files cannot be read (web demo).
class DesktopEntriesLoader {
  const DesktopEntriesLoader();

  Future<List<DesktopEntry>> load() async => const [
        DesktopEntry(
          id: 'oasis-web-terminal',
          name: 'Terminal',
          exec: '',
          categories: ['System'],
        ),
        DesktopEntry(
          id: 'oasis-web-files',
          name: 'Files',
          exec: '',
          categories: ['Utility'],
        ),
        DesktopEntry(
          id: 'oasis-web-browser',
          name: 'Web Browser',
          exec: '',
          categories: ['Network'],
        ),
        DesktopEntry(
          id: 'oasis-web-editor',
          name: 'Text Editor',
          exec: '',
          categories: ['Utility'],
        ),
      ];
}

class SysfsBatteryReader {
  const SysfsBatteryReader();

  Future<BatteryStatus?> read() async => null;
}

ProcessRunner processRunner() => (command, args) async => -1;

DesktopEntriesLoader defaultEntriesLoader() => const DesktopEntriesLoader();

SysfsBatteryReader defaultBatteryReader() => const SysfsBatteryReader();
