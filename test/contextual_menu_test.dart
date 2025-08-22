import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_desktop_context_menu/flutter_desktop_context_menu.dart';

void main() {
  const MethodChannel channel = MethodChannel('flutter_desktop_context_menu');

  TestWidgetsFlutterBinding.ensureInitialized();

  setUp(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
      return true; // Mock successful responses
    });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  group('Context Menu Tests', () {
    test('Menu creation with ICO icons should work', () {
      expect(() {
        Menu(
          items: [
            MenuItem(
              label: 'Test ICO Icon',
              icon: r"C:\Users\LordLux\Pictures\Icons\flutter.ico",
              onClick: (_) {},
            ),
          ],
        );
      }, returnsNormally);
    });

    test('Menu creation with PNG icons should work', () {
      expect(() {
        Menu(
          items: [
            MenuItem(
              label: 'Test PNG Icon',
              icon: r"C:\Users\LordLux\Pictures\Icons\flutter.png",
              onClick: (_) {},
            ),
          ],
        );
      }, returnsNormally);
    });

    test('Checkbox menu items with PNG icons should work', () {
      expect(() {
        Menu(
          items: [
            MenuItem.checkbox(
              label: 'Checkbox with PNG Icons',
              icon: r"C:\Users\LordLux\Pictures\Icons\flutter.png",
              checkedIcon: r"C:\Users\LordLux\Pictures\Icons\flutter.png",
              checked: false,
              onClick: (_) {},
            ),
          ],
        );
      }, returnsNormally);
    });

    test('Mixed icon types in same menu should work', () {
      expect(() {
        Menu(
          items: [
            MenuItem(
              label: 'ICO Item',
              icon: r"C:\Users\LordLux\Pictures\Icons\flutter.ico",
              onClick: (_) {},
            ),
            MenuItem(
              label: 'PNG Item',
              icon: r"C:\Users\LordLux\Pictures\Icons\flutter.png",
              onClick: (_) {},
            ),
            MenuItem.separator(),
            MenuItem.checkbox(
              label: 'Mixed Icons Checkbox',
              icon: r"C:\Users\LordLux\Pictures\Icons\flutter.png",
              checkedIcon: r"C:\Users\LordLux\Pictures\Icons\flutter.ico",
              checked: true,
              onClick: (_) {},
            ),
          ],
        );
      }, returnsNormally);
    });

    test('Menu items without icons should still work', () {
      expect(() {
        Menu(
          items: [
            MenuItem(
              label: 'No Icon Item',
              onClick: (_) {},
            ),
            MenuItem.separator(),
            MenuItem.checkbox(
              label: 'Checkbox No Icon',
              checked: false,
              onClick: (_) {},
            ),
          ],
        );
      }, returnsNormally);
    });
  });
}
