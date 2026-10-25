/// The clock the shell reads; tests pin it so panels and goldens are
/// deterministic.
DateTime Function() clockNow = DateTime.now;
