import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/services/compositor_service.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  final messenger =
      TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger;

  group('CompositorService', () {
    test('detects the compositor and sends commands', () async {
      String? last;
      messenger.setMockMessageHandler('oasis.wm', (message) async {
        last = utf8.decode(
            message!.buffer.asUint8List(message.offsetInBytes));
        return ByteData.view(
            Uint8List.fromList(utf8.encode('{"pong":true}')).buffer);
      });
      final service = CompositorService(messenger: messenger);
      await service.init();
      expect(service.available, isTrue);
      expect(last, contains('ping'));
      await service.focus(7);
      expect(last, contains('"focus"'));
      await service.close(7);
      expect(last, contains('"close"'));
    });

    test('marks itself unavailable without a native handler', () async {
      final service = CompositorService(
        messenger: messenger,
        channelName: 'oasis.test.missing',
      );
      await service.init();
      expect(service.available, isFalse);
    });

    test('updates window list on pushed events', () async {
      String? reply;
      messenger.setMockMessageHandler('oasis.wm', (message) async {
        return ByteData.view(
            Uint8List.fromList(utf8.encode('{"pong":true}')).buffer);
      });
      final service = CompositorService(messenger: messenger);
      var notifies = 0;
      service.addListener(() => notifies++);
      await service.init();

      // native -> dart push
      await messenger.handlePlatformMessage(
        'oasis.wm',
        ByteData.view(Uint8List.fromList(utf8.encode(jsonEncode({
          'type': 'windows',
          'windows': [
            {
              'id': 1,
              'title': 'Files',
              'appId': 'org.gnome.Nautilus',
              'focused': true
            },
            {'id': 2, 'title': 'Web', 'appId': 'org.mozilla.firefox'},
            {'id': 3},
          ],
        }))).buffer),
        (data) => reply = data == null
            ? null
            : utf8.decode(data.buffer.asUint8List()),
      );
      expect(service.windows.length, 3);
      expect(service.windows[0].title, 'Files');
      expect(service.windows[0].focused, isTrue);
      expect(service.windows[1].appId, 'org.mozilla.firefox');
      expect(service.windows[2].title, '');
      expect(service.windows[2].focused, isFalse);
      expect(notifies, greaterThan(0));
      await messenger.handlePlatformMessage('oasis.wm', null, (_) {});
      await messenger.handlePlatformMessage(
        'oasis.wm',
        ByteData.view(
            Uint8List.fromList(utf8.encode('{"type":"other"}')).buffer),
        (_) {},
      );
      expect(service.windows.length, 3);
    });

    test('send failures mark the channel unavailable', () async {
      final service = CompositorService(
        messenger: messenger,
        channelName: 'oasis.test.missing2',
      );
      await service.focus(1);
      await service.close(1);
      expect(service.available, isFalse);
    });
  });
}
