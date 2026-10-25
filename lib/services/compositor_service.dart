import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import '../models/compositor_window.dart';

/// Talks to the Oasis compositor over a JSON string channel. When the shell
/// runs inside the compositor the channel is live; under a regular GTK
/// embedder it is absent and [available] stays false.
class CompositorService extends ChangeNotifier {
  CompositorService({BinaryMessenger? messenger, this.channelName = 'oasis.wm'})
      : _channel = BasicMessageChannel<String>(
          channelName,
          const StringCodec(),
          binaryMessenger: messenger,
        );

  final String channelName;
  final BasicMessageChannel<String> _channel;

  bool _available = false;
  bool get available => _available;

  List<CompositorWindow> _windows = const [];
  List<CompositorWindow> get windows => _windows;

  Future<void> init() async {
    _channel.setMessageHandler(_onMessage);
    try {
      final reply = await _channel.send('{"method":"ping"}');
      _available = reply != null && reply.contains('pong');
    } on MissingPluginException {
      _available = false;
    }
  }

  Future<String> _onMessage(String? message) async {
    if (message == null) return '';
    final decoded = jsonDecode(message);
    if (decoded is! Map || decoded['type'] != 'windows') return '';
    final list = decoded['windows'];
    _windows = [
      if (list is List)
        for (final w in list)
          if (w is Map)
            CompositorWindow.fromJson(Map<String, dynamic>.from(w)),
    ];
    notifyListeners();
    return '';
  }

  Future<void> focus(int id) => _send('focus', id);
  Future<void> close(int id) => _send('close', id);

  Future<void> _send(String method, int id) async {
    try {
      await _channel.send('{"method":"$method","id":$id}');
    } on MissingPluginException {
      _available = false;
    }
  }
}
