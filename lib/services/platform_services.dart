export 'platform_services_io.dart'
    if (dart.library.html) 'platform_services_stub.dart'
    if (dart.library.js_interop) 'platform_services_stub.dart';
