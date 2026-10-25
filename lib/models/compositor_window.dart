/// A toplevel window managed by the Oasis compositor.
class CompositorWindow {
  final int id;
  final String title;
  final String appId;
  final bool focused;

  const CompositorWindow({
    required this.id,
    required this.title,
    required this.appId,
    this.focused = false,
  });

  factory CompositorWindow.fromJson(Map<String, dynamic> json) {
    return CompositorWindow(
      id: json['id'] as int? ?? 0,
      title: json['title'] as String? ?? '',
      appId: json['appId'] as String? ?? '',
      focused: json['focused'] as bool? ?? false,
    );
  }
}
