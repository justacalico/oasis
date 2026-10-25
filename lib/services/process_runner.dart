/// Runs a command and returns its exit code. The real implementation lives in
/// platform services; tests inject a fake.
typedef ProcessRunner = Future<int> Function(String command, List<String> args);
