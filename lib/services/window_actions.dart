export 'window_actions_io.dart'
    if (dart.library.html) 'window_actions_stub.dart'
    if (dart.library.js_interop) 'window_actions_stub.dart';
