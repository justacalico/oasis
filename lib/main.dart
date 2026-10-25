import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app.dart';
import 'services/window_service.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await initializeWindow();
  final prefs = await SharedPreferences.getInstance();
  runApp(OasisApp(prefs: prefs));
}
