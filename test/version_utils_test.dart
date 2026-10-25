import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/utils/version_utils.dart';

void main() {
  group('Version.parse', () {
    test('parses plain and v-prefixed versions', () {
      expect(Version.parse('1.2.3')!.core, [1, 2, 3]);
      expect(Version.parse('v1.2.3')!.core, [1, 2, 3]);
      expect(Version.parse('V2.0.0')!.core, [2, 0, 0]);
      expect(Version.parse('vv1.0.0')!.core, [1, 0, 0]);
      expect(Version.parse(' 1.2.3 ')!.core, [1, 2, 3]);
    });

    test('parses pre-release and build metadata', () {
      final v = Version.parse('1.2.3-alpha.1+build5')!;
      expect(v.core, [1, 2, 3]);
      expect(v.pre, ['alpha', '1']);
      expect(Version.parse('1.2.3+meta')!.pre, isNull);
    });

    test('rejects malformed input', () {
      for (final bad in [
        null,
        '',
        '1.2',
        '1.2.3.4',
        '1.2.x',
        '01.2.3',
        '1.2.3-',
        '1.2.3-a..b',
        '1.2.3-é',
        '1.2.3-01',
        'a.b.c',
      ]) {
        expect(Version.parse(bad), isNull, reason: bad ?? 'null');
      }
    });
  });

  group('compareTo', () {
    test('compares core', () {
      expect(Version.parse('1.2.3')!.compareTo(Version.parse('1.2.4')!), -1);
      expect(Version.parse('2.0.0')!.compareTo(Version.parse('1.9.9')!), 1);
      expect(Version.parse('1.2.3')!.compareTo(Version.parse('1.2.3')!), 0);
      expect(Version.parse('1.0.0')!.compareTo(Version.parse('1.0.1')!), -1);
    });

    test('release beats pre-release', () {
      expect(
        Version.parse('1.0.0')!.compareTo(Version.parse('1.0.0-beta')!),
        1,
      );
      expect(
        Version.parse('1.0.0-beta')!.compareTo(Version.parse('1.0.0')!),
        -1,
      );
    });

    test('pre-release ordering', () {
      expect(
        Version.parse('1.0.0-alpha')!.compareTo(Version.parse('1.0.0-beta')!),
        -1,
      );
      expect(
        Version.parse('1.0.0-2')!.compareTo(Version.parse('1.0.0-10')!),
        -1,
      );
      expect(
        Version.parse('1.0.0-1')!.compareTo(Version.parse('1.0.0-alpha')!),
        -1,
      );
      expect(
        Version.parse('1.0.0-alpha')!.compareTo(Version.parse('1.0.0-1')!),
        1,
      );
      expect(
        Version.parse('1.0.0-a.b')!.compareTo(Version.parse('1.0.0-a')!),
        1,
      );
      expect(
        Version.parse('1.0.0-2')!.compareTo(Version.parse('1.0.0-2')!),
        0,
      );
    });
  });

  test('toString round-trips', () {
    expect(Version.parse('1.2.3').toString(), '1.2.3');
    expect(Version.parse('1.2.3-rc.1').toString(), '1.2.3-rc.1');
  });

  test('equality and hashCode', () {
    expect(Version.parse('1.2.3'), Version.parse('v1.2.3'));
    expect(Version.parse('1.2.3'), isNot(Version.parse('1.2.4')));
    expect(Version.parse('1.2.3'), isNot(Version.parse('1.2.3-a')));
    expect(Version.parse('1.2.3').hashCode, Version.parse('v1.2.3').hashCode);
    expect(Version.parse('1.2.3'), isNot('x'));
  });
}
