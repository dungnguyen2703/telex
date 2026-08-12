// Draws the tray icons at runtime so the build needs no binary assets.
#pragma once

#include <windows.h>

#include <cstdint>

namespace app {

constexpr int kIconSize = 32;

// Renders the icon into a caller-supplied kIconSize * kIconSize BGRA buffer.
// Split out from MakeTrayIcon so the artwork can be rendered to a file and
// looked at without running the whole application.
void RenderIconPixels(bool enabled, uint32_t* pixels);

// Same artwork, as an HICON ready for the tray. Returns nullptr on failure.
HICON MakeTrayIcon(bool enabled);

}  // namespace app
