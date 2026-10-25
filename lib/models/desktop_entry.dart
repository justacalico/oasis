/// A parsed .desktop file describing an installed application.
class DesktopEntry {
  final String id;
  final String name;
  final String exec;
  final String? comment;
  final List<String> categories;

  const DesktopEntry({
    required this.id,
    required this.name,
    required this.exec,
    this.comment,
    this.categories = const [],
  });

  static DesktopEntry? parse(String id, String contents) {
    String? name;
    String? exec;
    String? comment;
    var hidden = false;
    var noDisplay = false;
    var inDesktopEntry = false;
    final categories = <String>[];

    for (final rawLine in contents.split('\n')) {
      final line = rawLine.trim();
      if (line.startsWith('[')) {
        inDesktopEntry = line == '[Desktop Entry]';
        continue;
      }
      if (!inDesktopEntry || line.isEmpty || line.startsWith('#')) continue;

      final eq = line.indexOf('=');
      if (eq == -1) continue;
      final key = line.substring(0, eq).trim();
      final value = line.substring(eq + 1).trim();

      // Localised keys like Name[de]= are ignored; the generic Name= wins.
      if (key.contains('[')) continue;
      switch (key) {
        case 'Name':
          name = value;
        case 'Exec':
          exec = value;
        case 'Comment':
          comment = value;
        case 'Categories':
          categories.addAll(
            value.split(';').where((c) => c.isNotEmpty),
          );
        case 'Hidden':
          hidden = value == 'true';
        case 'NoDisplay':
          noDisplay = value == 'true';
      }
    }

    if (hidden || noDisplay || name == null || name.isEmpty) return null;
    return DesktopEntry(
      id: id,
      name: name,
      exec: exec ?? '',
      comment: comment?.isEmpty ?? true ? null : comment,
      categories: List.unmodifiable(categories),
    );
  }

  /// The command tokens from the Exec line with field codes like %f stripped.
  List<String> get execCommand {
    final tokens = <String>[];
    var current = StringBuffer();
    var inQuotes = false;
    for (var i = 0; i < exec.length; i++) {
      final c = exec[i];
      if (c == '"') {
        inQuotes = !inQuotes;
        continue;
      }
      if (c == ' ' && !inQuotes) {
        if (current.isNotEmpty) {
          tokens.add(current.toString());
          current = StringBuffer();
        }
        continue;
      }
      current.write(c);
    }
    if (current.isNotEmpty) tokens.add(current.toString());
    return tokens.where((t) => !t.startsWith('%')).toList();
  }
}
