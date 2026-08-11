#include "icon.h"

#include <vector>

namespace app {
namespace {

// Anything left in this colour when drawing is done becomes transparent.
constexpr COLORREF kTransparentKey = RGB(255, 0, 255);
constexpr uint32_t kTransparentPixel = 0x00FF00FF;

// Draws into a DIB section and hands back both the bitmap and its pixels.
HBITMAP Render(bool enabled, HDC dc, void** bitsOut) {
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = kIconSize;
    info.bmiHeader.biHeight = -kIconSize;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, bitsOut, nullptr, 0);
    if (!bitmap) return nullptr;
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

    HBRUSH keyBrush = CreateSolidBrush(kTransparentKey);
    RECT full = {0, 0, kIconSize, kIconSize};
    FillRect(dc, &full, keyBrush);
    DeleteObject(keyBrush);

    // Red badge with a white V; when off, the same shape drained of colour.
    const COLORREF badge = enabled ? RGB(214, 40, 40) : RGB(120, 124, 130);
    const COLORREF glyph = enabled ? RGB(255, 255, 255) : RGB(220, 222, 226);
    HBRUSH badgeBrush = CreateSolidBrush(badge);
    HGDIOBJ oldBrush = SelectObject(dc, badgeBrush);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, 0, 0, kIconSize + 1, kIconSize + 1, 10, 10);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(badgeBrush);

    HFONT font = CreateFontW(34, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, glyph);
    RECT text = {0, -1, kIconSize, kIconSize};
    DrawTextW(dc, L"V", 1, &text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
    DeleteObject(font);

    // GDI drawing leaves the alpha channel at zero; set it explicitly.
    uint32_t* pixels = static_cast<uint32_t*>(*bitsOut);
    for (int i = 0; i < kIconSize * kIconSize; ++i) {
        if ((pixels[i] & 0x00FFFFFF) == kTransparentPixel) {
            pixels[i] = 0;
        } else {
            pixels[i] |= 0xFF000000;
        }
    }

    SelectObject(dc, oldBitmap);
    return bitmap;
}

}  // namespace

void RenderIconPixels(bool enabled, uint32_t* out) {
    HDC dc = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP bitmap = Render(enabled, dc, &bits);
    if (bitmap) {
        memcpy(out, bits, sizeof(uint32_t) * kIconSize * kIconSize);
        DeleteObject(bitmap);
    }
    DeleteDC(dc);
}

HICON MakeTrayIcon(bool enabled) {
    HDC dc = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP colour = Render(enabled, dc, &bits);
    if (!colour) {
        DeleteDC(dc);
        return nullptr;
    }

    std::vector<BYTE> maskBits(static_cast<size_t>(kIconSize * kIconSize) / 8, 0);
    HBITMAP mask = CreateBitmap(kIconSize, kIconSize, 1, 1, maskBits.data());

    ICONINFO iconInfo = {};
    iconInfo.fIcon = TRUE;
    iconInfo.hbmMask = mask;
    iconInfo.hbmColor = colour;
    HICON icon = CreateIconIndirect(&iconInfo);

    DeleteObject(mask);
    DeleteObject(colour);
    DeleteDC(dc);
    return icon;
}

}  // namespace app
