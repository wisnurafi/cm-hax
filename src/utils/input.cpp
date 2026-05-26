// utils/input.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "input.h"

namespace Input {

bool IsKeyDown(int vk) {
    if (vk == 0) return false;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool WasKeyJustPressed(int vk) {
    if (vk == 0) return false;
    static bool s_prev[256] = {};
    if (vk < 0 || vk >= 256) return false;
    bool down = IsKeyDown(vk);
    bool edge = down && !s_prev[vk];
    s_prev[vk] = down;
    return edge;
}

// Set of VKs we let the user bind. We intentionally exclude meta keys that
// would make the menu unusable (LWIN/RWIN/APPS/...) and the menu hotkeys
// themselves (INSERT/END) so users can't accidentally lock themselves out.
static bool IsBindableVK(int vk) {
    switch (vk) {
        case VK_LBUTTON: case VK_RBUTTON: case VK_MBUTTON:
        case VK_XBUTTON1: case VK_XBUTTON2:
            return true;
        case VK_TAB:
        case VK_SHIFT: case VK_CONTROL: case VK_MENU:
        case VK_LSHIFT: case VK_RSHIFT:
        case VK_LCONTROL: case VK_RCONTROL:
        case VK_LMENU: case VK_RMENU:
        case VK_SPACE: case VK_BACK:
        case VK_CAPITAL: case VK_OEM_3:
            return true;
        default: break;
    }
    if (vk >= '0' && vk <= '9') return true;
    if (vk >= 'A' && vk <= 'Z') return true;
    if (vk >= VK_F1 && vk <= VK_F24) return true;
    if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE) return true;
    return false;
}

int PollFirstPressed() {
    for (int vk = 1; vk < 256; vk++) {
        if (vk == VK_INSERT || vk == VK_END) continue; // reserved menu keys
        if (!IsBindableVK(vk)) continue;
        if (IsKeyDown(vk)) return vk;
    }
    return 0;
}

const char* DisplayName(int vk) {
    if (vk == 0) return "Unbound";
    switch (vk) {
        case VK_LBUTTON:  return "LMB";
        case VK_RBUTTON:  return "RMB";
        case VK_MBUTTON:  return "MMB";
        case VK_XBUTTON1: return "MOUSE4";
        case VK_XBUTTON2: return "MOUSE5";
        case VK_TAB:      return "TAB";
        case VK_SHIFT:    return "SHIFT";
        case VK_LSHIFT:   return "LSHIFT";
        case VK_RSHIFT:   return "RSHIFT";
        case VK_CONTROL:  return "CTRL";
        case VK_LCONTROL: return "LCTRL";
        case VK_RCONTROL: return "RCTRL";
        case VK_MENU:     return "ALT";
        case VK_LMENU:    return "LALT";
        case VK_RMENU:    return "RALT";
        case VK_SPACE:    return "SPACE";
        case VK_BACK:     return "BACKSPACE";
        case VK_CAPITAL:  return "CAPS";
        case VK_OEM_3:    return "`";
        case VK_INSERT:   return "INS";
        case VK_END:      return "END";
        default: break;
    }
    static char buf[8];
    if (vk >= '0' && vk <= '9') { buf[0] = (char)vk; buf[1] = '\0'; return buf; }
    if (vk >= 'A' && vk <= 'Z') { buf[0] = (char)vk; buf[1] = '\0'; return buf; }
    if (vk >= VK_F1 && vk <= VK_F24) {
        snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1);
        return buf;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        snprintf(buf, sizeof(buf), "NUM%d", vk - VK_NUMPAD0);
        return buf;
    }
    switch (vk) {
        case VK_MULTIPLY:  return "NUM*";
        case VK_ADD:       return "NUM+";
        case VK_SUBTRACT:  return "NUM-";
        case VK_DECIMAL:   return "NUM.";
        case VK_DIVIDE:    return "NUM/";
    }
    return "?";
}

} // namespace Input
