export 'window_service_io.dart'
    if (dart.library.html) 'window_service_stub.dart'
    if (dart.library.js_interop) 'window_service_stub.dart';
