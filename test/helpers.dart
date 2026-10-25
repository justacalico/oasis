import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/app.dart';
import 'package:oasis/models/app_update.dart';
import 'package:oasis/models/desktop_entry.dart';
import 'package:oasis/models/compositor_window.dart';
import 'package:oasis/services/app_launcher.dart';
import 'package:oasis/services/compositor_service.dart';
import 'package:oasis/services/battery.dart';
import 'package:oasis/services/platform_services.dart';
import 'package:oasis/services/session_actions.dart';
import 'package:oasis/services/version_checker.dart';
import 'package:oasis/state/app_state.dart';
import 'package:oasis/utils/clock.dart' as oasis_clock;
import 'package:oasis/views/shell.dart';
import 'package:shared_preferences/shared_preferences.dart';

Future<void> loadOasisFonts() async {
  final regular = FontLoader('Inter')
    ..addFont(rootBundle.load('assets/fonts/Inter-Regular.ttf'));
  await regular.load();
  final bold = FontLoader('Inter')
    ..addFont(rootBundle.load('assets/fonts/Inter-Bold.ttf'));
  await bold.load();
}

const fixtureEntries = [
  DesktopEntry(
    id: 'org.test.terminal',
    name: 'Terminal',
    exec: 'kgx',
    categories: ['System'],
  ),
  DesktopEntry(
    id: 'org.test.files',
    name: 'Files',
    exec: 'nautilus %U',
    categories: ['Utility'],
  ),
  DesktopEntry(
    id: 'org.test.browser',
    name: 'Web Browser',
    exec: 'firefox %u',
    categories: ['Network'],
  ),
];

class FakeEntriesLoader extends DesktopEntriesLoader {
  List<DesktopEntry> result;
  int calls = 0;

  FakeEntriesLoader(this.result) : super(const []);

  @override
  Future<List<DesktopEntry>> load() async {
    calls++;
    return result;
  }

  @override
  Future<List<DesktopEntry>> reload() async {
    calls++;
    return result;
  }
}

class FakeBatteryReader extends SysfsBatteryReader {
  final BatteryStatus? status;

  FakeBatteryReader(this.status) : super(powerSupplyPath: '');

  @override
  Future<BatteryStatus?> read() async => status;
}

class RecordingRunner {
  final List<String> calls = [];
  final List<int> exitCodes;
  int _i = 0;

  RecordingRunner([this.exitCodes = const [0]]);

  Future<int> call(String command, List<String> args) {
    calls.add([command, ...args].join(' '));
    final code = exitCodes[_i.clamp(0, exitCodes.length - 1)];
    _i++;
    return Future.value(code);
  }
}

class FakeVersionChecker extends VersionChecker {
  final AppUpdate? result;
  final Object? error;
  int calls = 0;

  FakeVersionChecker({this.result, this.error}) : super(client: null);

  @override
  Future<AppUpdate> check(String currentVersion) async {
    calls++;
    if (error != null) throw error!;
    return (result ?? const AppUpdate()).copyWith(
      currentVersion: currentVersion,
    );
  }
}

class FakeCompositorService extends CompositorService {
  List<CompositorWindow> result;
  final List<String> calls = [];

  FakeCompositorService([this.result = const []]);

  @override
  List<CompositorWindow> get windows => result;

  @override
  bool get available => true;

  @override
  Future<void> focus(int id) async {
    calls.add('focus:$id');
    result = [
      for (final w in result)
        CompositorWindow(
            id: w.id, title: w.title, appId: w.appId, focused: w.id == id),
    ];
    notifyListeners();
  }

  @override
  Future<void> close(int id) async {
    calls.add('close:$id');
    result = [for (final w in result) if (w.id != id) w];
    notifyListeners();
  }
}

ShellServices fakeServices({
  List<DesktopEntry>? entries,
  BatteryStatus? battery,
  List<int>? exitCodes,
  RecordingRunner? runner,
  DesktopEntriesLoader? loader,
  CompositorService? windows,
}) {
  final run = runner ?? RecordingRunner(exitCodes ?? const [0]);
  return ShellServices(
    entriesLoader: loader ?? FakeEntriesLoader(entries ?? fixtureEntries),
    appLauncher: AppLauncher(run.call),
    batteryReader: FakeBatteryReader(battery),
    sessionActions: SessionActions(run.call),
    windows: windows ?? FakeCompositorService(),
  );
}

Future<OasisState> pumpShell(
  WidgetTester tester, {
  Locale? locale,
  Size surface = const Size(1200, 800),
  ShellServices? services,
  VersionChecker? versionChecker,
  Map<String, Object> prefs = const {},
  TargetPlatform? platform = TargetPlatform.linux,
}) async {
  SharedPreferences.setMockInitialValues(prefs);
  final store = await SharedPreferences.getInstance();
  final state = OasisState(store, versionChecker: versionChecker);
  if (locale != null) state.setLocale(locale);
  oasis_clock.clockNow = () => DateTime(2026, 1, 15, 9, 41);
  addTearDown(() => oasis_clock.clockNow = DateTime.now);
  tester.view.physicalSize = surface;
  tester.view.devicePixelRatio = 1.0;
  addTearDown(tester.view.reset);
  await tester.pumpWidget(OasisApp(
    prefs: store,
    services: services ?? fakeServices(),
    stateOverride: state,
    platform: platform,
  ));
  await tester.pump();
  return state;
}

Future<void> pumpUntil(
  WidgetTester tester,
  bool Function() condition, {
  int maxPumps = 15,
  Duration step = const Duration(milliseconds: 100),
}) async {
  for (var i = 0; i < maxPumps && !condition(); i++) {
    await tester.pump(step);
  }
}
