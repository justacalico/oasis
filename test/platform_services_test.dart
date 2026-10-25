import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/models/desktop_entry.dart';
import 'package:oasis/services/app_launcher.dart';
import 'package:oasis/services/platform_services.dart';
import 'package:oasis/services/session_actions.dart';

import 'helpers.dart';

void main() {
  group('DesktopEntriesLoader', () {
    test('loads, sorts and skips non-entry files', () async {
      final dir = await Directory.systemTemp.createTemp('entries');
      addTearDown(() => dir.delete(recursive: true));
      File('${dir.path}/beta.desktop').writeAsStringSync(
        '[Desktop Entry]\nName=Beta\nExec=beta\n',
      );
      File('${dir.path}/alpha.desktop').writeAsStringSync(
        '[Desktop Entry]\nName=Alpha\nExec=alpha\n',
      );
      File('${dir.path}/hidden.desktop').writeAsStringSync(
        '[Desktop Entry]\nName=Hidden\nExec=x\nHidden=true\n',
      );
      File('${dir.path}/note.txt').writeAsStringSync('not an entry');

      final loader = DesktopEntriesLoader([dir]);
      final entries = await loader.load();
      expect(entries.map((e) => e.name), ['Alpha', 'Beta']);
      expect(entries.first.id, 'alpha');
    });

    test('ignores missing directories', () async {
      final loader = DesktopEntriesLoader([
        Directory('${Directory.systemTemp.path}/does-not-exist-oasis'),
      ]);
      expect(await loader.load(), isEmpty);
    });

    test('caches the scan until reloaded', () async {
      final dir = await Directory.systemTemp.createTemp('entries');
      addTearDown(() => dir.delete(recursive: true));
      File('${dir.path}/one.desktop').writeAsStringSync(
        '[Desktop Entry]\nName=One\nExec=one\n',
      );

      final loader = DesktopEntriesLoader([dir]);
      expect((await loader.load()).map((e) => e.name), ['One']);

      File('${dir.path}/two.desktop').writeAsStringSync(
        '[Desktop Entry]\nName=Two\nExec=two\n',
      );
      expect((await loader.load()).map((e) => e.name), ['One']);
      expect((await loader.reload()).map((e) => e.name), ['One', 'Two']);
    });

    test('skips entries that fail to read', () async {
      final dir = await Directory.systemTemp.createTemp('entries');
      addTearDown(() => dir.delete(recursive: true));
      File('${dir.path}/bad.desktop')
          .writeAsBytesSync([0xFF, 0xFE, 0x41]);
      File('${dir.path}/good.desktop').writeAsStringSync(
        '[Desktop Entry]\nName=Good\nExec=good\n',
      );

      final entries = await DesktopEntriesLoader([dir]).load();
      expect(entries.map((e) => e.name), ['Good']);
    });

    test('default loader points at the applications dirs', () {
      final loader = defaultEntriesLoader();
      expect(loader, isA<DesktopEntriesLoader>());
    });
  });

  group('SysfsBatteryReader', () {
    test('reads capacity and charging state', () async {
      final dir = await Directory.systemTemp.createTemp('power');
      addTearDown(() => dir.delete(recursive: true));
      final bat = Directory('${dir.path}/BAT0')..createSync();
      File('${bat.path}/capacity').writeAsStringSync('83\n');
      File('${bat.path}/status').writeAsStringSync('Discharging\n');
      Directory('${dir.path}/AC').createSync();

      final reader = SysfsBatteryReader(powerSupplyPath: dir.path);
      final status = await reader.read();
      expect(status!.capacity, 83);
      expect(status.charging, isFalse);
    });

    test('reports charging', () async {
      final dir = await Directory.systemTemp.createTemp('power');
      addTearDown(() => dir.delete(recursive: true));
      final bat = Directory('${dir.path}/BAT1')..createSync();
      File('${bat.path}/capacity').writeAsStringSync('100');
      File('${bat.path}/status').writeAsStringSync('Charging');

      final status =
          await SysfsBatteryReader(powerSupplyPath: dir.path).read();
      expect(status!.charging, isTrue);
    });

    test('returns null without a sysfs dir, battery or valid capacity',
        () async {
      expect(
        await SysfsBatteryReader(
          powerSupplyPath: '${Directory.systemTemp.path}/nope-oasis',
        ).read(),
        isNull,
      );

      final empty = await Directory.systemTemp.createTemp('power-empty');
      addTearDown(() => empty.delete(recursive: true));
      expect(
        await SysfsBatteryReader(powerSupplyPath: empty.path).read(),
        isNull,
      );

      final noCap = Directory('${empty.path}/BAT0')..createSync();
      expect(
        await SysfsBatteryReader(powerSupplyPath: empty.path).read(),
        isNull,
      );

      File('${noCap.path}/capacity').writeAsStringSync('junk');
      expect(
        await SysfsBatteryReader(powerSupplyPath: empty.path).read(),
        isNull,
      );

      File('${noCap.path}/capacity').writeAsStringSync('55');
      final status =
          await SysfsBatteryReader(powerSupplyPath: empty.path).read();
      expect(status!.capacity, 55);
      expect(status.charging, isFalse);
    });

    test('default reader targets sysfs', () {
      expect(defaultBatteryReader().powerSupplyPath, '/sys/class/power_supply');
    });
  });

  group('processRunner', () {
    test('returns exit code and -1 on failure', () async {
      final run = processRunner();
      expect(await run('true', []), 0);
      expect(await run('definitely-not-a-real-binary-oasis', []), -1);
    });
  });

  group('SessionActions', () {
    test('runs the session commands', () async {
      final runner = RecordingRunner();
      final session = SessionActions(runner.call);
      await session.lock();
      await session.suspend();
      await session.restart();
      await session.powerOff();
      expect(runner.calls, [
        'loginctl lock-session',
        'systemctl suspend',
        'systemctl reboot',
        'systemctl poweroff',
      ]);
    });
  });

  group('AppLauncher', () {
    test('launches via gtk-launch', () async {
      final runner = RecordingRunner();
      final launcher = AppLauncher(runner.call);
      const entry = DesktopEntry(id: 'org.test.app', name: 'App', exec: 'x');
      expect(await launcher.launch(entry), 0);
      expect(runner.calls, ['gtk-launch org.test.app']);
    });

    test('falls back to exec when gtk-launch fails', () async {
      final runner = RecordingRunner([1, 0]);
      final launcher = AppLauncher(runner.call);
      const entry = DesktopEntry(
        id: 'org.test.app',
        name: 'App',
        exec: 'myapp --open %f',
      );
      expect(await launcher.launch(entry), 0);
      expect(runner.calls, [
        'gtk-launch org.test.app',
        'myapp --open',
      ]);
    });

    test('returns failure code when both attempts fail', () async {
      final runner = RecordingRunner([1, 2]);
      final launcher = AppLauncher(runner.call);
      const entry = DesktopEntry(
        id: 'org.test.app',
        name: 'App',
        exec: 'myapp',
      );
      expect(await launcher.launch(entry), 2);
    });

    test('returns failure code directly when exec is empty', () async {
      final runner = RecordingRunner([1]);
      final launcher = AppLauncher(runner.call);
      const entry = DesktopEntry(id: 'x', name: 'x', exec: '');
      expect(await launcher.launch(entry), 1);
      expect(runner.calls.length, 1);
    });
  });
}
