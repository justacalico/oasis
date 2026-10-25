/// Battery status read from sysfs on Linux, null when there is no battery.
class BatteryStatus {
  final int capacity;
  final bool charging;

  const BatteryStatus({required this.capacity, required this.charging});
}
