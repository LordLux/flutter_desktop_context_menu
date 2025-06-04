import 'dart:ui';

import 'menu.dart';
import 'placement.dart';

mixin MenuBehavior {
  Future<void> popUp(
    Menu menu, {
    Offset? position,
    Placement placement = Placement.topLeft,
  });
}
