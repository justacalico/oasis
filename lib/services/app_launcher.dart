import '../models/desktop_entry.dart';
import 'process_runner.dart';

/// Launches desktop entries. Prefers gtk-launch so activation, env vars and
/// startup notification behave like a real desktop shell.
class AppLauncher {
  final ProcessRunner run;

  const AppLauncher(this.run);

  /// Returns the exit code of the launch attempt.
  Future<int> launch(DesktopEntry entry) {
    final gtkLaunch = run('gtk-launch', [entry.id]);
    return gtkLaunch.then((code) {
      if (code == 0 || entry.execCommand.isEmpty) return code;
      final tokens = entry.execCommand;
      return run(tokens.first, tokens.sublist(1));
    });
  }
}
