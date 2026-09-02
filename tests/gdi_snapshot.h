#pragma once

// Offscreen capture of the production GDI renderer. No game/window hooks,
// desktop capture, external image libraries, or approximate HTML mockups.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct InventoryTestBitmapHeader {
    DWORD size;
    LONG width;
    LONG height;
    WORD planes;
    WORD bitsPerPixel;
    DWORD compression;
    DWORD imageSize;
    LONG xPixelsPerMeter;
    LONG yPixelsPerMeter;
    DWORD colorsUsed;
    DWORD colorsImportant;
};
struct InventoryTestBitmapInfo {
    InventoryTestBitmapHeader header;
    DWORD colors[1];
};
extern "C" __declspec(dllimport) HBITMAP WINAPI CreateDIBSection(
    HDC, const InventoryTestBitmapInfo*, UINT, void**, HANDLE, DWORD);
extern "C" __declspec(dllimport) BOOL WINAPI GdiFlush();

static void InventoryTestAppendBigEndian(std::vector<unsigned char>& bytes,
    std::uint32_t value)
{
    bytes.push_back(static_cast<unsigned char>(value >> 24));
    bytes.push_back(static_cast<unsigned char>(value >> 16));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
    bytes.push_back(static_cast<unsigned char>(value));
}

static std::uint32_t InventoryTestCrc(const unsigned char* bytes, std::size_t count)
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static void InventoryTestPngChunk(std::vector<unsigned char>& png,
    const char* name, const std::vector<unsigned char>& data)
{
    InventoryTestAppendBigEndian(png, static_cast<std::uint32_t>(data.size()));
    const auto crcStart = png.size();
    for (int i = 0; i < 4; ++i) png.push_back(name[i]);
    png.insert(png.end(), data.begin(), data.end());
    InventoryTestAppendBigEndian(png,
        InventoryTestCrc(png.data() + crcStart, 4 + data.size()));
}

static bool InventoryTestWritePng(const char* path, const unsigned char* bgra,
    int width, int height)
{
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<std::size_t>(height) * (1 + width * 3));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0); // PNG filter: None.
        for (int x = 0; x < width; ++x) {
            const auto pixel = bgra + (static_cast<std::size_t>(y) * width + x) * 4;
            raw.push_back(pixel[2]);
            raw.push_back(pixel[1]);
            raw.push_back(pixel[0]);
        }
    }

    // Zlib with stored DEFLATE blocks keeps this diagnostic helper dependency-free.
    std::vector<unsigned char> compressed{0x78, 0x01};
    for (std::size_t offset = 0; offset < raw.size();) {
        const auto length = static_cast<unsigned int>(
            (std::min)(raw.size() - offset, static_cast<std::size_t>(65535)));
        compressed.push_back(offset + length == raw.size() ? 1 : 0);
        compressed.push_back(static_cast<unsigned char>(length));
        compressed.push_back(static_cast<unsigned char>(length >> 8));
        compressed.push_back(static_cast<unsigned char>(~length));
        compressed.push_back(static_cast<unsigned char>((~length) >> 8));
        compressed.insert(compressed.end(), raw.begin() + offset,
            raw.begin() + offset + length);
        offset += length;
    }
    std::uint32_t adlerLow = 1, adlerHigh = 0;
    for (const auto value : raw) {
        adlerLow = (adlerLow + value) % 65521u;
        adlerHigh = (adlerHigh + adlerLow) % 65521u;
    }
    InventoryTestAppendBigEndian(compressed, (adlerHigh << 16) | adlerLow);

    std::vector<unsigned char> png{137, 80, 78, 71, 13, 10, 26, 10};
    std::vector<unsigned char> header;
    InventoryTestAppendBigEndian(header, static_cast<std::uint32_t>(width));
    InventoryTestAppendBigEndian(header, static_cast<std::uint32_t>(height));
    header.insert(header.end(), {8, 2, 0, 0, 0}); // 8-bit RGB.
    InventoryTestPngChunk(png, "IHDR", header);
    InventoryTestPngChunk(png, "IDAT", compressed);
    InventoryTestPngChunk(png, "IEND", {});
    FILE* file = nullptr;
    if (fopen_s(&file, path, "wb") != 0 || !file) return false;
    const bool wrote = fwrite(png.data(), 1, png.size(), file) == png.size();
    return fclose(file) == 0 && wrote;
}

static bool InventoryTestRenderPng(const char* path, int width, int height,
    void (*render)(HDC, int, int))
{
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    InventoryTestBitmapInfo info{};
    info.header.size = sizeof(InventoryTestBitmapHeader);
    info.header.width = width;
    info.header.height = -height; // top-down
    info.header.planes = 1;
    info.header.bitsPerPixel = 32;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, 0, &pixels, nullptr, 0);
    if (!bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        DeleteDC(dc);
        return false;
    }
    const HGDIOBJ previous = SelectObject(dc, bitmap);
    render(dc, width, height);
    GdiFlush();
    const bool wrote = InventoryTestWritePng(path,
        static_cast<const unsigned char*>(pixels), width, height);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return wrote;
}
