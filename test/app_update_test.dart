import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/models/app_update.dart';

void main() {
  test('copyWith keeps and replaces fields', () {
    const a = AppUpdate(
      currentVersion: '1.0.0',
      latestVersion: '1.1.0',
      updateAvailable: true,
      releaseUrl: 'https://example.com',
    );
    expect(a.copyWith().latestVersion, '1.1.0');
    final b = a.copyWith(latestVersion: '2.0.0', updateAvailable: false);
    expect(b.latestVersion, '2.0.0');
    expect(b.updateAvailable, isFalse);
    expect(b.currentVersion, '1.0.0');
    expect(b.releaseUrl, 'https://example.com');
  });

  test('equality and hashCode', () {
    const a = AppUpdate(currentVersion: '1', latestVersion: '2');
    const b = AppUpdate(currentVersion: '1', latestVersion: '2');
    expect(a, b);
    expect(a.hashCode, b.hashCode);
    expect(a, isNot(const AppUpdate(currentVersion: '9')));
    expect(a, isNot('x'));
  });
}
