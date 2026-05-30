// injector/gui.h
// ImGui-based GUI for the injector. D3D11 standalone window.
#pragma once

namespace GUI {
    // Runs the injector GUI. Blocks until the window is closed.
    // Returns 0 on success, 1 on failure.
    int Run();
}
