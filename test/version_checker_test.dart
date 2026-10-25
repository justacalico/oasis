import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:oasis/services/version_checker.dart';

MockClient clientOf(Object Function(Uri url, int call) handler) {
  var calls = 0;
  return MockClient((request) async {
    calls++;
    final result = handler(request.url, calls);
    if (result is int) {
      return http.Response('error', result);
    }
    if (result is String) {
      return http.Response(result, 200);
    }
    return http.Response(jsonEncode(result), 200);
  });
}

void main() {
  test('picks the highest semver tag across releases', () async {
    final checker = VersionChecker(
      client: clientOf((url, call) => [
        {'tag_name': 'v1.2.0'},
        {'tag_name': 'nightly'},
        {'tag_name': 'v1.10.0'},
        {'tag_name': 'v1.9.9'},
      ]),
    );
    final update = await checker.check('1.0.0');
    expect(update.updateAvailable, isTrue);
    expect(update.latestVersion, '1.10.0');
    expect(
      update.releaseUrl,
      'https://gitlab.com/HttpAnimations/oasis/-/releases/v1.10.0',
    );
    expect(update.currentVersion, '1.0.0');
  });

  test('reports up to date when installed is newer', () async {
    final checker = VersionChecker(
      client: clientOf((url, call) => [
        {'tag_name': 'v0.5.0'},
      ]),
    );
    final update = await checker.check('9.9.9');
    expect(update.updateAvailable, isFalse);
    expect(update.latestVersion, '0.5.0');
  });

  test('returns no-update result when no semver tags exist', () async {
    final checker = VersionChecker(
      client: clientOf((url, call) => [
        {'tag_name': 'nightly'},
        {'name': 'missing tag'},
        'not a map',
      ]),
    );
    final update = await checker.check('1.0.0');
    expect(update.updateAvailable, isFalse);
    expect(update.releaseUrl, VersionChecker.releasesUrl);
  });

  test('treats unparseable current version as needing the update', () async {
    final checker = VersionChecker(
      client: clientOf((url, call) => [
        {'tag_name': 'v1.0.0'},
      ]),
    );
    final update = await checker.check('dev');
    expect(update.updateAvailable, isTrue);
    expect(update.latestVersion, '1.0.0');
  });

  test('paginates while pages are full', () async {
    final checker = VersionChecker(
      client: clientOf((url, call) {
        expect(url.queryParameters['page'], '$call');
        if (call == 1) {
          return List.generate(
            100,
            (i) => {'tag_name': 'v0.0.${i + 1}'},
          );
        }
        return [
          {'tag_name': 'v2.0.0'}
        ];
      }),
    );
    final update = await checker.check('1.0.0');
    expect(update.latestVersion, '2.0.0');
  });

  test('throws on non-200 responses', () async {
    final checker = VersionChecker(client: clientOf((url, call) => 503));
    expect(
      () => checker.check('1.0.0'),
      throwsA(isA<VersionCheckException>()),
    );
  });

  test('throws on unexpected response shape', () async {
    final checker = VersionChecker(
      client: clientOf((url, call) => {'tag_name': 'v1.0.0'}),
    );
    expect(
      () => checker.check('1.0.0'),
      throwsA(isA<VersionCheckException>()),
    );
  });

  test('wraps network failures in VersionCheckException', () async {
    final checker = VersionChecker(
      client: MockClient((request) async => throw http.ClientException('down')),
    );
    expect(
      () => checker.check('1.0.0'),
      throwsA(isA<VersionCheckException>()),
    );
  });

  test('exception toString includes the message', () {
    expect(
      const VersionCheckException('nope').toString(),
      'VersionCheckException: nope',
    );
  });

  test('close releases the owned client', () {
    VersionChecker().close();
    final injected = MockClient((request) async => http.Response('[]', 200));
    VersionChecker(client: injected).close();
  });
}