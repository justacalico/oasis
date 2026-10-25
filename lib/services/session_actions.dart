import 'process_runner.dart';

/// Session management actions backed by loginctl/systemctl on Linux.
class SessionActions {
  final ProcessRunner run;

  const SessionActions(this.run);

  Future<int> lock() => run('loginctl', ['lock-session']);
  Future<int> suspend() => run('systemctl', ['suspend']);
  Future<int> restart() => run('systemctl', ['reboot']);
  Future<int> powerOff() => run('systemctl', ['poweroff']);
}
