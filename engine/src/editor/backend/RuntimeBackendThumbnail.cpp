// do@Redlive

#include "dopch.h"

#include "RuntimeBackend.h"

#include "runtime/resource/resource_manager.h"
#include "runtime/resource/asset/asset_manager.h"
#include "runtime/resource/asset/types/mesh_asset.h"
#include "runtime/resource/asset/types/tiled_map_asset.h"
#include "runtime/resource/asset/types/tileset_asset.h"
#include "runtime/resource/asset/types/material_asset.h"

#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace dodoe;

namespace cakery {

namespace {

constexpr int kThumbnailSize = 96;

String GetBase64Encode(const DynamicArray<UInt8>& data)
{
    static const char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (Size_t i = 0; i < data.size(); i += 3) {
        const UInt32 a = data[i];
        const UInt32 b = i + 1 < data.size() ? data[i + 1] : 0;
        const UInt32 c = i + 2 < data.size() ? data[i + 2] : 0;
        const UInt32 n = (a << 16) | (b << 8) | c;
        out.push_back(kTable[(n >> 18) & 0x3F]);
        out.push_back(kTable[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < data.size() ? kTable[(n >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < data.size() ? kTable[n & 0x3F] : '=');
    }
    return out;
}

bool GetImagePixels(const FsPath& path, int& outWidth, int& outHeight,
                    DynamicArray<UInt8>& outPixels)
{
    int channels = 0;
    unsigned char* data = stbi_load(path.string().c_str(), &outWidth, &outHeight,
                                    &channels, STBI_rgb_alpha);
    if (!data) {
        return false;
    }
    outPixels.assign(data, data + static_cast<Size_t>(outWidth) * outHeight * 4);
    stbi_image_free(data);
    return true;
}

FsPath GetResolvedImagePath(const FsPath& assetDir,
                                   const String& sourcePath,
                                   const String& imagePath)
{
    if (FsPath(imagePath).is_absolute()) {
        return FsPath(imagePath);
    }
    return assetDir / FsPath(sourcePath).parent_path() /
           FsPath(imagePath);
}

bool GetMeshThumbnailRgba(const MeshAsset* mesh, DynamicArray<UInt8>& rgba)
{
    const auto data = mesh->getData();
    if (!data || data->vertices.empty() || data->indices.size() < 3) {
        return false;
    }

    const auto& verts = data->vertices;
    const auto& indices = data->indices;
    const int k = kThumbnailSize;

    const Size_t n = verts.size();
    DynamicArray<Vector3f> placed(n);
    if (data->sections.empty()) {
        for (Size_t i = 0; i < n; ++i) {
            placed[i] = verts[i].position;
        }
    } else {
        for (const MeshSection& section : data->sections) {
            const Size_t base = section.vertex_base;
            const Size_t count = section.vertex_count;
            if (base >= n || count > n - base) {
                continue;
            }
            for (Size_t i = base; i < base + count; ++i) {
                const Vector3f& v = verts[i].position;
                const Vector4f p = section.world * Vector4f(v.x, v.y, v.z, 1.0f);
                placed[i] = Vector3f(p.x, p.y, p.z);
            }
        }
    }

    DynamicArray<float> zbuf(static_cast<Size_t>(k) * k, -std::numeric_limits<float>::max());
    rgba.assign(static_cast<Size_t>(k) * k * 4, 0);

    const float rx = 0.7f;
    const float ry = -0.8f;
    const float cx = std::cos(rx), sx = std::sin(rx);
    const float cy = std::cos(ry), sy = std::sin(ry);

    float minX = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    DynamicArray<Vector3f> view;
    view.reserve(n);
    for (Size_t i = 0; i < n; ++i) {
        const Vector3f& v = placed[i];
        const float x = v.x * cy + v.z * sy;
        const float z = -v.x * sy + v.z * cy;
        const float y2 = v.y * cx - z * sx;
        const float z2 = v.y * sx + z * cx;
        view.push_back(Vector3f(x, y2, z2));
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y2);
        maxY = std::max(maxY, y2);
    }

    const float spanX = std::max(1e-6f, maxX - minX);
    const float spanY = std::max(1e-6f, maxY - minY);
    const float margin = static_cast<float>(k) * 0.12f;
    const float scale = std::min((static_cast<float>(k) - 2.0f * margin) / spanX,
                                 (static_cast<float>(k) - 2.0f * margin) / spanY);
    const float ox = static_cast<float>(k) * 0.5f - (minX + maxX) * 0.5f * scale;
    const float oy = static_cast<float>(k) * 0.5f + (minY + maxY) * 0.5f * scale;

    const Vector3f light = Math::Normalize(Vector3f(0.35f, 0.55f, 0.75f));
    const Size_t triCount = indices.size() / 3;
    for (Size_t t = 0; t < triCount; ++t) {
        const UInt32 i0 = indices[t * 3 + 0];
        const UInt32 i1 = indices[t * 3 + 1];
        const UInt32 i2 = indices[t * 3 + 2];
        if (i0 >= n || i1 >= n || i2 >= n) {
            continue;
        }

        const Vector3f& va = view[i0];
        const Vector3f& vb = view[i1];
        const Vector3f& vc = view[i2];
        const float ax = ox + va.x * scale;
        const float ay = oy - va.y * scale;
        const float bx = ox + vb.x * scale;
        const float by = oy - vb.y * scale;
        const float cxp = ox + vc.x * scale;
        const float cyp = oy - vc.y * scale;

        const Vector3f e1 = vb - va;
        const Vector3f e2 = vc - va;
        Vector3f faceN = Math::Cross(e1, e2);
        const float len = Math::Length(faceN);
        if (len < 1e-6f) {
            continue;
        }
        faceN = faceN / len;
        if (faceN.z <= 0.0f) {
            continue;
        }

        const float shade = 0.4f + 0.6f * std::max(0.0f, Math::Dot(faceN, light));
        const UInt8 val = static_cast<UInt8>(
            std::min(255.0f, std::max(0.0f, shade * 255.0f)));

        const int minPx = std::max(0, static_cast<int>(std::floor(std::min({ax, bx, cxp}))));
        const int minPy = std::max(0, static_cast<int>(std::floor(std::min({ay, by, cyp}))));
        const int maxPx = std::min(k - 1, static_cast<int>(std::ceil(std::max({ax, bx, cxp}))));
        const int maxPy = std::min(k - 1, static_cast<int>(std::ceil(std::max({ay, by, cyp}))));
        if (minPx > maxPx || minPy > maxPy) {
            continue;
        }

        const float denom0 = (by - cyp) * (ax - cxp) + (cxp - bx) * (ay - cyp);
        const float denom1 = (cyp - ay) * (bx - cxp) + (ax - cxp) * (by - cyp);
        if (std::abs(denom0) < 1e-9f || std::abs(denom1) < 1e-9f) {
            continue;
        }

        for (int py = minPy; py <= maxPy; ++py) {
            for (int px = minPx; px <= maxPx; ++px) {
                const float x = static_cast<float>(px) + 0.5f;
                const float y = static_cast<float>(py) + 0.5f;
                const float w0 = ((by - cyp) * (x - cxp) + (cxp - bx) * (y - cyp)) / denom0;
                const float w1 = ((cyp - ay) * (x - cxp) + (ax - cxp) * (y - cyp)) / denom1;
                const float w2 = 1.0f - w0 - w1;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                    continue;
                }
                const float depth = va.z * w0 + vb.z * w1 + vc.z * w2;
                const Size_t idx = static_cast<Size_t>(py) * k + px;
                if (depth <= zbuf[idx]) {
                    continue;
                }
                zbuf[idx] = depth;
                UInt8* out = &rgba[idx * 4];
                out[0] = val;
                out[1] = val;
                out[2] = val;
                out[3] = 255;
            }
        }
    }
    return true;
}

void BlitTileIntoCanvas(DynamicArray<UInt8>& canvas, int canvasSize,
                        const DynamicArray<UInt8>& src, int srcWidth, int srcHeight,
                        UInt32 srcX, UInt32 srcY, UInt32 tileWidth, UInt32 tileHeight,
                        int dstX, int dstY, float scale)
{
    for (UInt32 ty = 0; ty < tileHeight; ++ty) {
        for (UInt32 tx = 0; tx < tileWidth; ++tx) {
            const UInt32 px = srcX + tx;
            const UInt32 py = srcY + ty;
            if (px >= static_cast<UInt32>(srcWidth) || py >= static_cast<UInt32>(srcHeight)) {
                continue;
            }
            const Size_t srcIdx = (static_cast<Size_t>(py) * srcWidth + px) * 4;
            const int dx = static_cast<int>((static_cast<float>(dstX + tx)) * scale);
            const int dy = static_cast<int>((static_cast<float>(dstY + ty)) * scale);
            if (dx < 0 || dy < 0 || dx >= canvasSize || dy >= canvasSize) {
                continue;
            }
            const Size_t dstIdx = (static_cast<Size_t>(dy) * canvasSize + dx) * 4;
            canvas[dstIdx + 0] = src[srcIdx + 0];
            canvas[dstIdx + 1] = src[srcIdx + 1];
            canvas[dstIdx + 2] = src[srcIdx + 2];
            canvas[dstIdx + 3] = 255;
        }
    }
}

bool GetTiledMapThumbnailRgba(const TiledMapAsset* map, const FsPath& assetDir,
                              DynamicArray<UInt8>& rgba)
{
    const UInt32 mapWidth = map->getMapWidth();
    const UInt32 mapHeight = map->getMapHeight();
    const UInt32 tileWidth = map->getTileWidth();
    const UInt32 tileHeight = map->getTileHeight();
    if (mapWidth == 0 || mapHeight == 0 || tileWidth == 0 || tileHeight == 0) {
        return false;
    }

    struct LoadedTileset {
        const TiledMapTilesetData* data;
        DynamicArray<UInt8> pixels;
        int imageWidth = 0;
        int imageHeight = 0;
    };
    DynamicArray<LoadedTileset> tilesets;
    for (const auto& ts : map->getTilesets()) {
        if (ts.image_path.empty() || ts.tile_width == 0 || ts.tile_height == 0) {
            continue;
        }
        const FsPath imagePath =
            GetResolvedImagePath(assetDir, map->getSourcePath(), ts.image_path);
        LoadedTileset loaded;
        loaded.data = &ts;
        if (!GetImagePixels(imagePath, loaded.imageWidth, loaded.imageHeight, loaded.pixels)) {
            continue;
        }
        tilesets.push_back(std::move(loaded));
    }
    if (tilesets.empty()) {
        return false;
    }

    const int srcWidth = static_cast<int>(mapWidth) * tileWidth;
    const int srcHeight = static_cast<int>(mapHeight) * tileHeight;
    const int k = kThumbnailSize;
    const float scale = std::min(static_cast<float>(k - 8) / static_cast<float>(srcWidth),
                                 static_cast<float>(k - 8) / static_cast<float>(srcHeight));
    rgba.assign(static_cast<Size_t>(k) * k * 4, 0);

    for (const auto& layer : map->getLayers()) {
        if (!layer.visible || layer.tiles.empty()) {
            continue;
        }
        const UInt32 layerWidth = layer.width == 0 ? mapWidth : layer.width;
        const UInt32 layerHeight = layer.height == 0 ? mapHeight : layer.height;
        for (UInt32 ty = 0; ty < layerHeight; ++ty) {
            for (UInt32 tx = 0; tx < layerWidth; ++tx) {
                const UInt32 gid =
                    layer.tiles[static_cast<Size_t>(ty) * layerWidth + tx] & 0x0FFFFFFFu;
                if (gid == 0) {
                    continue;
                }
                for (const auto& tileset : tilesets) {
                    const UInt32 firstGid = tileset.data->first_gid;
                    if (gid < firstGid || gid >= firstGid + tileset.data->tile_count) {
                        continue;
                    }
                    const UInt32 index = gid - firstGid;
                    const UInt32 columns =
                        tileset.data->columns == 0 ? 1 : tileset.data->columns;
                    const UInt32 col = index % columns;
                    const UInt32 row = index / columns;
                    const UInt32 srcX = tileset.data->margin + col * (tileset.data->tile_width + tileset.data->spacing);
                    const UInt32 srcY = tileset.data->margin + row * (tileset.data->tile_height + tileset.data->spacing);
                    BlitTileIntoCanvas(rgba, k, tileset.pixels, tileset.imageWidth,
                                       tileset.imageHeight, srcX, srcY,
                                       tileset.data->tile_width, tileset.data->tile_height,
                                       static_cast<int>(tx * tileWidth),
                                       static_cast<int>(ty * tileHeight), scale);
                    break;
                }
            }
        }
    }
    return true;
}

void DrawGridPixel(DynamicArray<UInt8>& canvas, int canvasSize, int px, int py)
{
    const Size_t idx = (static_cast<Size_t>(py) * canvasSize + px) * 4;
    const UInt8 overlay = 30;
    canvas[idx + 0] = static_cast<UInt8>(std::min(255, canvas[idx + 0] / 2 + overlay / 2));
    canvas[idx + 1] = static_cast<UInt8>(std::min(255, canvas[idx + 1] / 2 + overlay / 2));
    canvas[idx + 2] = static_cast<UInt8>(std::min(255, canvas[idx + 2] / 2 + overlay / 2));
    canvas[idx + 3] = 255;
}

bool GetTilesetThumbnailRgba(const TilesetAsset* tileset, const FsPath& assetDir,
                             DynamicArray<UInt8>& rgba)
{
    if (tileset->getImagePath().empty()) {
        return false;
    }
    const FsPath imagePath =
        GetResolvedImagePath(assetDir, tileset->getSourcePath(), tileset->getImagePath());
    int width = 0, height = 0;
    DynamicArray<UInt8> pixels;
    if (!GetImagePixels(imagePath, width, height, pixels)) {
        return false;
    }

    const int k = kThumbnailSize;
    const float scale = std::min(static_cast<float>(k - 8) / static_cast<float>(width),
                                 static_cast<float>(k - 8) / static_cast<float>(height));
    rgba.assign(static_cast<Size_t>(k) * k * 4, 0);
    for (int py = 0; py < k; ++py) {
        for (int px = 0; px < k; ++px) {
            const int sx = static_cast<int>(static_cast<float>(px) / scale);
            const int sy = static_cast<int>(static_cast<float>(py) / scale);
            if (sx >= width || sy >= height) {
                continue;
            }
            const Size_t srcIdx = (static_cast<Size_t>(sy) * width + sx) * 4;
            const Size_t dstIdx = (static_cast<Size_t>(py) * k + px) * 4;
            rgba[dstIdx + 0] = pixels[srcIdx + 0];
            rgba[dstIdx + 1] = pixels[srcIdx + 1];
            rgba[dstIdx + 2] = pixels[srcIdx + 2];
            rgba[dstIdx + 3] = 255;
        }
    }

    if (tileset->getTileWidth() > 0 && tileset->getTileHeight() > 0) {
        const int stepX = static_cast<int>(
            static_cast<float>(tileset->getTileWidth() + tileset->getSpacing()) * scale);
        const int stepY = static_cast<int>(
            static_cast<float>(tileset->getTileHeight() + tileset->getSpacing()) * scale);
        const int originX = static_cast<int>(static_cast<float>(tileset->getMargin()) * scale);
        const int originY = originX;
        if (stepX > 0) {
            for (int x = originX + stepX; x < k; x += stepX) {
                for (int py = 0; py < k; ++py) {
                    DrawGridPixel(rgba, k, x, py);
                }
            }
        }
        if (stepY > 0) {
            for (int y = originY + stepY; y < k; y += stepY) {
                for (int px = 0; px < k; ++px) {
                    DrawGridPixel(rgba, k, px, y);
                }
            }
        }
    }
    return true;
}

bool GetMaterialThumbnailRgba(const MaterialAsset* material, DynamicArray<UInt8>& rgba)
{
    const int k = kThumbnailSize;
    rgba.assign(static_cast<Size_t>(k) * k * 4, 0);
    const auto& color = material->getColor();
    const UInt8 r = static_cast<UInt8>(std::min(255.0f, std::max(0.0f, color.x * 255.0f)));
    const UInt8 g = static_cast<UInt8>(std::min(255.0f, std::max(0.0f, color.y * 255.0f)));
    const UInt8 b = static_cast<UInt8>(std::min(255.0f, std::max(0.0f, color.z * 255.0f)));
    for (Size_t i = 0; i < rgba.size(); i += 4) {
        rgba[i + 0] = r;
        rgba[i + 1] = g;
        rgba[i + 2] = b;
        rgba[i + 3] = 255;
    }
    return true;
}

} // namespace

bool RuntimeBackend::queryAssetThumbnail(const String& path, int size,
                                               Json& out) const
{
    out = nullptr;
    if (!m_booted || path.empty()) {
        return false;
    }
    if (size <= 0) {
        size = kThumbnailSize;
    }

    const auto info = m_assetDatabase ? m_assetDatabase->findByPath(path) : std::nullopt;
    if (!info) {
        return false;
    }

    auto* assetManager = ResourceManager::Self().getAssetManager();
    if (!assetManager) {
        return false;
    }

    const UUID assetId(info->uuid);
    DynamicArray<UInt8> rgba;
    bool generated = false;
    if (info->type == "Mesh") {
        const auto* mesh = assetManager->loadAssetSync<MeshAsset>(assetId);
        generated = mesh && GetMeshThumbnailRgba(mesh, rgba);
    } else if (info->type == "TiledMap") {
        const auto* map = assetManager->loadAssetSync<TiledMapAsset>(assetId);
        generated = map && GetTiledMapThumbnailRgba(map, assetManager->getAssetDir(), rgba);
    } else if (info->type == "Tileset") {
        const auto* tileset = assetManager->loadAssetSync<TilesetAsset>(assetId);
        generated = tileset && GetTilesetThumbnailRgba(tileset, assetManager->getAssetDir(), rgba);
    } else if (info->type == "Material") {
        const auto* material = assetManager->loadAssetSync<MaterialAsset>(assetId);
        generated = material && GetMaterialThumbnailRgba(material, rgba);
    }
    if (!generated) {
        return false;
    }

    out["width"] = kThumbnailSize;
    out["height"] = kThumbnailSize;
    out["data"] = GetBase64Encode(rgba);
    return true;
}

} // namespace cakery
