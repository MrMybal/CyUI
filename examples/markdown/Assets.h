#pragma once
#include <cyui/modules/DocumentIO.h>
#include "Platform.h"
#include "Raster.h"
#include <fstream>
#include <unordered_map>
#include <stdexcept>

namespace native_assets {
namespace fs = std::filesystem;
inline std::vector<unsigned char> read(const fs::path &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0 || input.tellg() > 64 * 1024 * 1024)
        throw std::runtime_error("Unable to import file (64 MiB maximum)");
    std::vector<unsigned char> bytes(std::size_t(input.tellg()));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size())))
        throw std::runtime_error("Unable to read attachment");
    return bytes;
}
inline std::string escaped(const std::string &text) {
    std::string out;
    for (char c : text) {
        if (c == '[' || c == ']' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}
struct Asset {
    cyui::ItemId id = 0;
    std::string name, uri;
    std::vector<unsigned char> bytes;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture{nullptr, SDL_DestroyTexture};
    cyui::ImageData image;
    cyui::TranscriptAttachment attachment() const {
        return {id, name, uri, bytes.size(), image.texture != 0};
    }
};
class Store {
    SDL_Renderer *renderer;
    cyui::ItemId next = 1;
    std::unordered_map<std::string, std::unique_ptr<Asset>> items;
    std::unordered_map<std::string, std::string> local;
    void decode(Asset &asset) {
        const auto decoded = raster::decode(asset.bytes.data(), asset.bytes.size());
        if (decoded.pixels.empty())
            return;
        SDL_Surface *surface =
            SDL_CreateSurfaceFrom(decoded.width, decoded.height, SDL_PIXELFORMAT_RGBA32,
                                  const_cast<unsigned char *>(decoded.pixels.data()), decoded.width * 4);
        if (!surface)
            return;
        asset.texture.reset(SDL_CreateTextureFromSurface(renderer, surface));
        SDL_DestroySurface(surface);
        if (asset.texture) {
            SDL_SetTextureBlendMode(asset.texture.get(), SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(asset.texture.get(), SDL_SCALEMODE_LINEAR);
            asset.image = {ImTextureID(reinterpret_cast<std::uintptr_t>(asset.texture.get())), decoded.width,
                           decoded.height};
        }
    }

  public:
    explicit Store(SDL_Renderer *target) : renderer(target) {}
    Asset &add(std::string name, std::vector<unsigned char> bytes) {
        auto asset = std::make_unique<Asset>();
        asset->id = next++;
        asset->name = std::move(name);
        asset->bytes = std::move(bytes);
        auto extension = platform::utf8(platform::path(asset->name).extension());
        if (extension.size() > 12)
            extension.clear();
        asset->uri = "asset://" + std::to_string(asset->id) + extension;
        decode(*asset);
        auto *result = asset.get();
        items.emplace(asset->uri, std::move(asset));
        return *result;
    }
    Asset &import(const fs::path &path) {
        return add(platform::utf8(path.filename()), read(path));
    }
    const Asset *find(const std::string &uri) const {
        auto it = items.find(uri);
        return it == items.end() ? nullptr : it->second.get();
    }
    cyui::ImageData image(const std::string &uri) const {
        if (const auto *asset = find(uri))
            return asset->image;
        auto known = local.find(uri);
        if (known != local.end())
            if (const auto *asset = find(known->second))
                return asset->image;
        return {};
    }
    void prepare(const std::string &markdown, const fs::path &base) {
        cyui::Document document;
        document.setMarkdown(markdown);
        for (const auto &block : document.blocks())
            for (const auto &run : block.inlines) {
                if (run.kind != cyui::DocumentInlineKind::image || find(run.target) ||
                    local.count(run.target) || run.target.find("://") != std::string::npos ||
                    run.target.empty())
                    continue;
                try {
                    auto path = platform::path(run.target);
                    if (path.is_relative())
                        path = base / path;
                    local[run.target] = import(path).uri;
                } catch (...) {
                    local[run.target] = "";
                }
            }
    }
    void resetLocal() {
        local.clear();
    }
    std::string markup(const Asset &asset) const {
        return std::string(asset.image.texture ? "![" : "[") + escaped(asset.name) + "](" + asset.uri + ")";
    }
    // Writes referenced assets next to a plain .md file; no CyMD package/container involved.
    std::string exportMarkdown(std::string text, const fs::path &destination) const {
        for (const auto &[source, uri] : local) {
            if (uri.empty())
                continue;
            for (const auto &wrapped : {"(" + source + ")", "(<" + source + ">)"}) {
                const auto replacement = "(" + uri + ")";
                for (std::size_t at = 0; (at = text.find(wrapped, at)) != std::string::npos;
                     at += replacement.size())
                    text.replace(at, wrapped.size(), replacement);
            }
        }
        for (const auto &[uri, asset] : items) {
            if (text.find(uri) == std::string::npos)
                continue;
            const auto name =
                "cyui-" + std::to_string(asset->id) + platform::utf8(platform::path(asset->name).extension());
            const auto target = destination.parent_path() / "assets" / platform::path(name);
            fs::create_directories(target.parent_path());
            // Existing different content gets another name, so imported media never clobbers a user's file.
            auto safe = target;
            int suffix = 0;
            while (fs::exists(safe) && read(safe) != asset->bytes) {
                safe = target.parent_path() /
                       platform::path(platform::utf8(target.stem()) + "-" + std::to_string(++suffix) +
                                      platform::utf8(target.extension()));
            }
            if (!fs::exists(safe)) {
                std::ofstream output(safe, std::ios::binary);
                output.write(reinterpret_cast<const char *>(asset->bytes.data()),
                             std::streamsize(asset->bytes.size()));
                output.close();
                if (!output)
                    throw std::runtime_error("Unable to save attachment");
            }
            const auto replacement = std::string("assets/") + platform::utf8(safe.filename());
            for (std::size_t at = 0; (at = text.find(uri, at)) != std::string::npos; at += replacement.size())
                text.replace(at, uri.size(), replacement);
        }
        return text;
    }
    Asset &sample() {
        const auto &p = cyui::palette();
        const ImU32 colors[]{p.bg2, p.accent, p.info, p.success};
        std::vector<ImU32> pixels(480 * 240);
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 480; ++x)
                pixels[y * 480 + x] = colors[(x / 120 + y / 80) % 4];
        auto bytes = raster::png(reinterpret_cast<const unsigned char *>(pixels.data()), 480, 240);
        return add("palette.png", std::move(bytes));
    }
};
} // namespace native_assets
