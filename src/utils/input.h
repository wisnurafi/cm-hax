// utils/input.h
// Centralized keyboard/mouse helpers. Used by the aim activation system,
// menu hotkeys, and any future bind (triggerbot, panic key, etc).
//
// Bind values are Win32 virtual-key codes. 0 means "unbound".
#pragma once

namespace Input {
    // True while the key is physically held. Treats VK_LBUTTON / VK_RBUTTON /
    // VK_MBUTTON / VK_XBUTTON1 / VK_XBUTTON2 the same as keyboard keys.
    bool IsKeyDown(int vk);

    // Returns true exactly once on the frame the key transitions from up
    // to down (per-vk edge state is tracked internally).
    bool WasKeyJustPressed(int vk);

    // Polls every supported VK code and returns the first one currently held.
    // Returns 0 when nothing matches. Used by the keybind widget.
    int PollFirstPressed();

    // Short, capitalized display string for a VK code (e.g. "LMB", "RMB",
    // "SHIFT", "Q", "F4"). Returns "Unbound" for 0, "?" for unknown codes.
    const char* DisplayName(int vk);
}
