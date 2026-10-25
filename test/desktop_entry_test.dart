import 'package:flutter_test/flutter_test.dart';
import 'package:oasis/models/desktop_entry.dart';

void main() {
  test('parses a standard desktop file', () {
    final e = DesktopEntry.parse('org.test.app', '''
[Desktop Entry]
Type=Application
Name=Test App
Comment=A test
Exec=/usr/bin/test --flag %U
Categories=Utility;Development;
Name[de]=Testen
''')!;
    expect(e.id, 'org.test.app');
    expect(e.name, 'Test App');
    expect(e.exec, '/usr/bin/test --flag %U');
    expect(e.comment, 'A test');
    expect(e.categories, ['Utility', 'Development']);
  });

  test('ignores keys outside [Desktop Entry]', () {
    final e = DesktopEntry.parse('x', '''
[Other]
Name=Wrong
[Desktop Entry]
Name=Right
Exec=run
''')!;
    expect(e.name, 'Right');
  });

  test('returns null for hidden, no-display and unnamed entries', () {
    expect(
      DesktopEntry.parse('x', '[Desktop Entry]\nName=A\nHidden=true\nExec=x'),
      isNull,
    );
    expect(
      DesktopEntry.parse('x', '[Desktop Entry]\nName=A\nNoDisplay=true\nExec=x'),
      isNull,
    );
    expect(
      DesktopEntry.parse('x', '[Desktop Entry]\nExec=x'),
      isNull,
    );
    expect(
      DesktopEntry.parse('x', '[Desktop Entry]\nName=\nExec=x'),
      isNull,
    );
  });

  test('handles missing optional fields and stray lines', () {
    final e = DesktopEntry.parse('x', '''
# comment
[Desktop Entry]
Name=Minimal
badline
=novalue
Comment=
''')!;
    expect(e.exec, '');
    expect(e.comment, isNull);
    expect(e.categories, isEmpty);
  });

  test('execCommand strips field codes and handles quotes', () {
    const e = DesktopEntry(
      id: 'x',
      name: 'x',
      exec: 'env FOO=1 "/opt/my app/run" %f %u',
    );
    expect(e.execCommand, ['env', 'FOO=1', '/opt/my app/run']);
  });

  test('execCommand with empty exec', () {
    const e = DesktopEntry(id: 'x', name: 'x', exec: '');
    expect(e.execCommand, isEmpty);
  });
}
