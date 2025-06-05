#include "include/flutter_desktop_context_menu/flutter_desktop_context_menu_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <codecvt>
#include <map>
#include <memory>
#include <sstream>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

using flutter::EncodableList;
using flutter::EncodableMap;
using flutter::EncodableValue;
using namespace Gdiplus;

typedef enum _PREFERRED_APP_MODE {
    PreferredAppModeDefault,
    PreferredAppModeAllowDark,
    PreferredAppModeForceDark,
    PreferredAppModeForceLight,
    PreferredAppModeMax
} PREFERRED_APP_MODE;

typedef PREFERRED_APP_MODE(WINAPI* PFN_SET_PREFERRED_APP_MODE)(PREFERRED_APP_MODE appMode);
typedef void(WINAPI* PFN_ALLOW_DARK_MODE_FOR_WINDOW)(HWND hwnd, BOOL allow);
typedef BOOL(WINAPI* PFN_SHOULD_APPS_USE_DARK_MODE)();

PFN_SET_PREFERRED_APP_MODE SetPreferredAppMode = nullptr;
PFN_ALLOW_DARK_MODE_FOR_WINDOW AllowDarkModeForWindow = nullptr;
PFN_SHOULD_APPS_USE_DARK_MODE ShouldAppsUseDarkMode = nullptr;

void InitDarkMode() {
  HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

  if (hUxtheme) {
    // Ordinal 135 is SetPreferredAppMode in Win10 1903+
    SetPreferredAppMode = reinterpret_cast<PFN_SET_PREFERRED_APP_MODE>(
      GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135))
    );

    // Ordinal 133 is AllowDarkModeForWindow
    AllowDarkModeForWindow = reinterpret_cast<PFN_ALLOW_DARK_MODE_FOR_WINDOW>(
      GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133))
    );

    // Ordinal 132 is ShouldAppsUseDarkMode
    ShouldAppsUseDarkMode = reinterpret_cast<PFN_SHOULD_APPS_USE_DARK_MODE>(
      GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132))
    );

    // Force dark mode for the application
    if (SetPreferredAppMode) SetPreferredAppMode(PreferredAppModeForceDark);
  }
}

void EnableDarkModeForContextMenu(HWND hwnd) {
  // Enable dark mode for the window
  if (AllowDarkModeForWindow) AllowDarkModeForWindow(hwnd, TRUE);
  

  // For Windows 10 1809+
  BOOL darkModeEnabled = TRUE;
  DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkModeEnabled, sizeof(darkModeEnabled));

  // Update window to apply changes
  SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

HICON LoadIconFromFile(LPCWSTR filePath, int size) {
    return (HICON)LoadImage(
        NULL,              // No module per file esterni
        filePath,          // Percorso del file .ico
        IMAGE_ICON,        // Tipo di immagine
        size, size,        // Dimensioni desiderate
        LR_LOADFROMFILE |  // Carica da file
        LR_DEFAULTCOLOR    // Usa colori predefiniti
    );
}

HBITMAP IconToBitmap(HICON hIcon, int width, int height) {
    // Ottieni DC dello schermo
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    
    // Crea bitmap compatibile
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    
    // Riempi con colore background del menu
    HBRUSH hBrush = CreateSolidBrush(GetSysColor(COLOR_MENU));
    RECT rect = {0, 0, width, height};
    FillRect(hMemDC, &rect, hBrush);
    DeleteObject(hBrush);
    
    // Disegna l'icona sulla bitmap
    DrawIconEx(hMemDC, 0, 0, hIcon, width, height, 0, NULL, DI_NORMAL);
    
    // Pulizia
    SelectObject(hMemDC, hOldBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
    
    return hBitmap;
}

HBITMAP LoadIconFromFile(const std::wstring& iconPath) {
  HBITMAP hBitmap = (HBITMAP)LoadImageW(
      NULL,                      // hInstance - NULL for files
      iconPath.c_str(),          // Image path
      IMAGE_BITMAP,                // Image type
      16, 16,                    // Image dimensions
      LR_LOADFROMFILE | LR_DEFAULTSIZE            // Load from file
  );
  return hBitmap;
}

namespace {

const EncodableValue* ValueOrNull(const EncodableMap& map, const char* key) {
  auto it = map.find(EncodableValue(key));
  if (it == map.end()) {
    return nullptr;
  }
  return &(it->second);
}

std::unique_ptr<flutter::MethodChannel<EncodableValue>, std::default_delete<flutter::MethodChannel<EncodableValue>>> channel = nullptr;

class FlutterDesktopContextMenuPlugin : public flutter::Plugin {
  
  public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

    FlutterDesktopContextMenuPlugin(flutter::PluginRegistrarWindows* registrar);

    virtual ~FlutterDesktopContextMenuPlugin();

  private:
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> g_converter;

    flutter::PluginRegistrarWindows* registrar;

    // The ID of the WindowProc delegate registration.
    int window_proc_id = -1;

    HMENU hMenu;

    void FlutterDesktopContextMenuPlugin::_CreateMenu(
      HMENU menu,
      UINT_PTR menuId,
      EncodableMap args
    );

    // Called for top-level WindowProc delegation.
    std::optional<LRESULT> FlutterDesktopContextMenuPlugin::HandleWindowProc(
      HWND hwnd,
      UINT message,
      WPARAM wparam,
      LPARAM lparam
    );
    
    HWND FlutterDesktopContextMenuPlugin::GetMainWindow();
    void FlutterDesktopContextMenuPlugin::PopUp(
      const flutter::MethodCall<EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<EncodableValue>> result
    );

    // Called when a method is called on this plugin's channel from Dart.
    void HandleMethodCall(
      const flutter::MethodCall<EncodableValue>& method_call,
      std::unique_ptr<flutter::MethodResult<EncodableValue>> result
    );
};

// static:

void FlutterDesktopContextMenuPlugin::RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar) {
  channel = std::make_unique<flutter::MethodChannel<EncodableValue>>(
    registrar->messenger(),
    "flutter_desktop_context_menu",
    &flutter::StandardMethodCodec::GetInstance()
  );

  auto plugin = std::make_unique<FlutterDesktopContextMenuPlugin>(registrar);

  channel->SetMethodCallHandler(
    [plugin_pointer = plugin.get()](const auto& call, auto result) {
      plugin_pointer->HandleMethodCall(call, std::move(result));
    }
  );

  registrar->AddPlugin(std::move(plugin));
}

FlutterDesktopContextMenuPlugin::FlutterDesktopContextMenuPlugin(flutter::PluginRegistrarWindows* registrar) : registrar(registrar) {
  InitDarkMode();
  
  window_proc_id = registrar->RegisterTopLevelWindowProcDelegate(
    [this](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
      return HandleWindowProc(hwnd, message, wparam, lparam);
    }
  );
}

FlutterDesktopContextMenuPlugin::~FlutterDesktopContextMenuPlugin() {}

void FlutterDesktopContextMenuPlugin::_CreateMenu(
  HMENU menu,
  UINT_PTR menuId,
  EncodableMap args
) {
  EncodableList items = std::get<EncodableList>(args.at(EncodableValue("items")));

  // Store the menuId in the application defined `dwMenuData` field. This
  // allows us to retain the dart defined item id for submenu items.
  MENUINFO menuInfo;
  menuInfo.cbSize = sizeof(MENUINFO);
  menuInfo.fMask = MIM_MENUDATA;
  menuInfo.dwMenuData = menuId;
  SetMenuInfo(menu, &menuInfo);

  int count = GetMenuItemCount(menu);
  for (int i = 0; i < count; i++) {
    // always remove at 0 because they shift every time
    RemoveMenu(menu, 0, MF_BYPOSITION);
  }

  // Get map of items
  for (EncodableValue item_value : items) {
    EncodableMap item_map = std::get<EncodableMap>(item_value);
    // ID
    int id = std::get<int>(item_map.at(EncodableValue("id")));
    // Type
    std::string type = std::get<std::string>(item_map.at(EncodableValue("type")));
    // Label
    std::string label = std::get<std::string>(item_map.at(EncodableValue("label")));
    // Checked
    auto* checked = std::get_if<bool>(ValueOrNull(item_map, "checked"));
    // Disabled
    bool disabled = std::get<bool>(item_map.at(EncodableValue("disabled")));
    // Icon path
    auto* icon_path_value = ValueOrNull(item_map, "icon");
    std::string icon_path = icon_path_value ? std::get<std::string>(*icon_path_value) : "";

    UINT_PTR item_id = id;
    UINT uFlags = MF_STRING;

    if (disabled) uFlags |= MF_GRAYED;

    if (type.compare("separator") == 0) {
      AppendMenuW(menu, MF_SEPARATOR, item_id, NULL);
      
    } else {
      if (type.compare("checkbox") == 0) {
        if (checked == nullptr) {
          // skip
        } else {
          uFlags |= (*checked == true ? MF_CHECKED : MF_UNCHECKED);
        }
        
      } else if (type.compare("submenu") == 0) {
        uFlags |= MF_POPUP;
        HMENU sub_menu = ::CreatePopupMenu();
        
        _CreateMenu(
          sub_menu,
          item_id,
          std::get<EncodableMap>(item_map.at(EncodableValue("submenu")))
        );
        
        item_id = reinterpret_cast<UINT_PTR>(sub_menu);
      }
      
      // If an icon path is provided, load the bitmap and use it in the menu item
      if (!icon_path.empty()) {
        // Convert to wstring for Windows API
        std::wstring w_icon_path = g_converter.from_bytes(icon_path);
        
        HICON hIcon = NULL;
        
        // Check file extension .ico
        if (w_icon_path.substr(w_icon_path.find_last_of(L".") + 1) == L"ico") {
          hIcon = (HICON)LoadImage(
            NULL,
            w_icon_path.c_str(),
            IMAGE_ICON,
            16, 16,
            LR_LOADFROMFILE
          );
        }
        
        HBITMAP hBitmap = NULL;
        
        if (hIcon) {
          hBitmap = IconToBitmap(hIcon, 16, 16);
          DestroyIcon(hIcon); // Pulisci l'icona originale
        }

        if (hBitmap) {
          std::wstring w_label = g_converter.from_bytes(label);
          
          MENUITEMINFOW mii = {0};
          mii.cbSize = sizeof(MENUITEMINFOW);
          
          if (type.compare("submenu") == 0) {
            mii.fMask = MIIM_STRING | MIIM_BITMAP | MIIM_STATE | MIIM_SUBMENU;
            mii.hSubMenu = reinterpret_cast<HMENU>(item_id); // The submenu handle
          } else {
            mii.fMask = MIIM_STRING | MIIM_ID | MIIM_BITMAP | MIIM_STATE;
            mii.wID = static_cast<UINT>(item_id);
          }
          
          mii.fType = MFT_STRING;
          mii.fState = (disabled ? MFS_GRAYED : MFS_ENABLED) | (checked && *checked ? MFS_CHECKED : 0);
          mii.dwTypeData = const_cast<LPWSTR>(w_label.c_str());
          mii.hbmpItem = hBitmap;
          InsertMenuItemW(menu, GetMenuItemCount(menu), TRUE, &mii);
        } else {
          // Fallback to AppendMenuW if image loading fails
          AppendMenuW(menu, uFlags, item_id, g_converter.from_bytes(label).c_str());
        }
      } else {
        AppendMenuW(menu, uFlags, item_id, g_converter.from_bytes(label).c_str());
      }
    }
  }
}

std::optional<LRESULT> FlutterDesktopContextMenuPlugin::HandleWindowProc(
  HWND hWnd,
  UINT message,
  WPARAM wParam,
  LPARAM lParam
) {
  std::optional<LRESULT> result;
  if (message == WM_COMMAND) {
    flutter::EncodableMap eventData = flutter::EncodableMap();
    eventData[flutter::EncodableValue("id")] = flutter::EncodableValue((int)LOWORD(wParam));

    channel->InvokeMethod("onMenuItemClick", std::make_unique<flutter::EncodableValue>(eventData));
    
  } else if (message == WM_MENUSELECT) {
    auto itemParam = LOWORD(wParam);
    flutter::EncodableMap eventData = flutter::EncodableMap();

    if (itemParam < 256) {
      auto subMenuHandle = GetSubMenu((HMENU)lParam, itemParam);

      MENUINFO menuInfo;
      menuInfo.cbSize = sizeof(MENUINFO);
      menuInfo.fMask = MIM_MENUDATA;
      auto getSubMenuInfoSuccess = GetMenuInfo(subMenuHandle, &menuInfo);
      
      if (!getSubMenuInfoSuccess) return std::nullopt;

      eventData[flutter::EncodableValue("id")] = flutter::EncodableValue((int)menuInfo.dwMenuData);
    } else {
      eventData[flutter::EncodableValue("id")] = flutter::EncodableValue((int)itemParam);
    }

    channel->InvokeMethod("onMenuItemHighlight", std::make_unique<flutter::EncodableValue>(eventData));
  }

  return std::nullopt;
}

HWND FlutterDesktopContextMenuPlugin::GetMainWindow() {
  return ::GetAncestor(registrar->GetView()->GetNativeWindow(), GA_ROOT);
}

void FlutterDesktopContextMenuPlugin::PopUp(
  const flutter::MethodCall<EncodableValue>& method_call,
  std::unique_ptr<flutter::MethodResult<EncodableValue>> result
) {
  HWND hWnd = GetMainWindow();
  EnableDarkModeForContextMenu(hWnd);

  // SetForegroundWindow(hWnd);

  const EncodableMap& args = std::get<EncodableMap>(*method_call.arguments());

  std::string placement = std::get<std::string>(args.at(EncodableValue("placement")));
  double device_pixel_ratio = std::get<double>(args.at(flutter::EncodableValue("devicePixelRatio")));

  hMenu = CreatePopupMenu();
  _CreateMenu(
    hMenu,
    0,
    std::get<EncodableMap>(args.at(EncodableValue("menu")))
  );

  double x, y;

  UINT uFlags = TPM_TOPALIGN;

  POINT cursorPos;
  GetCursorPos(&cursorPos);
  x = cursorPos.x;
  y = cursorPos.y;

  if (args.find(EncodableValue("position")) != args.end()) {
    const EncodableMap& position = std::get<EncodableMap>(args.at(EncodableValue("position")));

    double position_x = std::get<double>(position.at(flutter::EncodableValue("x")));
    double position_y = std::get<double>(position.at(flutter::EncodableValue("y")));

    RECT window_rect, client_rect;
    TITLEBARINFOEX title_bar_info;

    GetWindowRect(hWnd, &window_rect);
    GetClientRect(hWnd, &client_rect);
    title_bar_info.cbSize = sizeof(TITLEBARINFOEX);
    ::SendMessage(hWnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)&title_bar_info);
    int32_t title_bar_height = 
      title_bar_info.rcTitleBar.bottom == 0
        ? 0
        : title_bar_info.rcTitleBar.bottom - title_bar_info.rcTitleBar.top;

    int border_thickness = ((window_rect.right - window_rect.left) - client_rect.right) / 2;

    x = static_cast<double>((position_x * device_pixel_ratio) + (window_rect.left + border_thickness));
    y = static_cast<double>((position_y * device_pixel_ratio) + (window_rect.top + title_bar_height));
  }

  if (placement.compare("topLeft") == 0) {
    uFlags = TPM_BOTTOMALIGN | TPM_RIGHTALIGN;
  } else if (placement.compare("topRight") == 0) {
    uFlags = TPM_BOTTOMALIGN | TPM_LEFTALIGN;
  } else if (placement.compare("bottomLeft") == 0) {
    uFlags = TPM_TOPALIGN | TPM_RIGHTALIGN;
  } else if (placement.compare("bottomRight") == 0) {
    uFlags = TPM_TOPALIGN | TPM_LEFTALIGN;
  }

  TrackPopupMenu(
    hMenu,
    uFlags,
    static_cast<int>(x),
    static_cast<int>(y),
    0,
    hWnd,
    NULL
  );

  result->Success(EncodableValue(true));
}

void FlutterDesktopContextMenuPlugin::HandleMethodCall(
  const flutter::MethodCall<EncodableValue>& method_call,
  std::unique_ptr<flutter::MethodResult<EncodableValue>> result
) {
  if (method_call.method_name().compare("popUp") == 0) {
    PopUp(method_call, std::move(result));
  } else {
    result->NotImplemented();
  }
}

}  // namespace

void FlutterDesktopContextMenuPluginRegisterWithRegistrar(FlutterDesktopPluginRegistrarRef registrar) {
  FlutterDesktopContextMenuPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarManager::GetInstance() ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar)
  );
}
