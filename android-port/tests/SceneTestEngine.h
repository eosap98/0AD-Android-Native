// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Minimal engine services for testing the real AndroidSceneBuffer.cpp on the
// host. These record allocation/lifetime and draw commands; they do not model a GPU.
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace SceneTest
{
inline int textures = 0, framebuffers = 0, warnings = 0;
inline bool depthTexture = true, coreDepth = false;
}
inline bool ogl_HaveVersion(int, int) { return SceneTest::coreDepth; }
inline bool ogl_HaveExtension(const char*) { return SceneTest::depthTexture; }
#define LOGWARNING(...) do { ++SceneTest::warnings; } while (false)
#define LOGMESSAGE(...) do {} while (false)

struct CColor
{
    float r = 0, g = 0, b = 0, a = 0;
    CColor() = default;
    CColor(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
};
struct CRect
{
    float left = 0, top = 0, right = 0, bottom = 0;
    CRect() = default;
    CRect(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}
};
namespace Renderer::Backend
{
enum class Backend { GL, DUMMY };
enum class Format { R8G8B8A8_UNORM, D16_UNORM };
namespace Sampler
{
enum class Filter { LINEAR, NEAREST };
enum class AddressMode { CLAMP_TO_EDGE };
struct Desc { Filter filter; AddressMode address; };
inline Desc MakeDefaultSampler(Filter filter, AddressMode address) { return {filter, address}; }
}
class ITexture
{
public:
    enum Usage : std::uint32_t { COLOR_ATTACHMENT = 1, SAMPLED = 2, DEPTH_STENCIL_ATTACHMENT = 4 };
    ITexture(std::uint32_t w, std::uint32_t h) : width(w), height(h) { ++SceneTest::textures; }
    ~ITexture() { assert(references == 0); --SceneTest::textures; }
    std::uint32_t width, height;
    int references = 0;
};
enum class AttachmentLoadOp { CLEAR, DONT_CARE };
enum class AttachmentStoreOp { STORE, DONT_CARE };
struct SColorAttachment
{
    ITexture* texture;
    AttachmentLoadOp loadOp;
    AttachmentStoreOp storeOp;
    CColor clearColor;
};
struct SDepthStencilAttachment
{
    ITexture* texture;
    AttachmentLoadOp loadOp;
    AttachmentStoreOp storeOp;
};
class IFramebuffer
{
public:
    IFramebuffer(ITexture* color, ITexture* depth) : color(color), depth(depth)
    {
        ++color->references; ++depth->references; ++SceneTest::framebuffers;
    }
    ~IFramebuffer() { --color->references; --depth->references; --SceneTest::framebuffers; }
    std::uint32_t GetWidth() const { return color->width; }
    std::uint32_t GetHeight() const { return color->height; }
    ITexture* color;
    ITexture* depth;
};
class IDevice
{
public:
    struct Capabilities { std::uint32_t maxTextureSize = 4096; } capabilities;
    Backend backend = Backend::GL;
    bool failFramebuffer = false;
    int failTextureCall = -1, textureCalls = 0, framebufferCalls = 0;
    std::vector<Sampler::Filter> filters;
    const Capabilities& GetCapabilities() const { return capabilities; }
    Backend GetBackend() const { return backend; }
    std::unique_ptr<ITexture> CreateTexture2D(const char*, std::uint32_t, Format,
        std::uint32_t width, std::uint32_t height, const Sampler::Desc& sampler)
    {
        filters.push_back(sampler.filter);
        if (++textureCalls == failTextureCall) return nullptr;
        return std::make_unique<ITexture>(width, height);
    }
    std::unique_ptr<IFramebuffer> CreateFramebuffer(const char*, SColorAttachment* color, SDepthStencilAttachment* depth)
    {
        ++framebufferCalls;
        assert(color->loadOp == AttachmentLoadOp::CLEAR && color->storeOp == AttachmentStoreOp::STORE);
        assert(depth->loadOp == AttachmentLoadOp::CLEAR && depth->storeOp == AttachmentStoreOp::DONT_CARE);
        assert(color->clearColor.a == 1);
        if (failFramebuffer) return nullptr;
        return std::make_unique<IFramebuffer>(color->texture, depth->texture);
    }
};
}
class CTexture
{
public:
    explicit CTexture(std::unique_ptr<Renderer::Backend::ITexture> texture) : texture(std::move(texture)) {}
    std::uint32_t GetWidth() const { return texture->width; }
    std::uint32_t GetHeight() const { return texture->height; }
    std::unique_ptr<Renderer::Backend::ITexture> texture;
};
using CTexturePtr = std::shared_ptr<CTexture>;
class CTextureManager
{
public:
    CTexturePtr WrapBackendTexture(std::unique_ptr<Renderer::Backend::ITexture> texture)
    {
        return std::make_shared<CTexture>(std::move(texture));
    }
};
class CCanvas2D
{
public:
    int draws = 0;
    CRect destination, source;
    CColor multiply, add;
    void DrawTexture(const CTexturePtr& texture, CRect destination, CRect source,
        CColor multiply, CColor add, float grayscale)
    {
        assert(texture && grayscale == 0);
        ++draws;
        this->destination = destination; this->source = source;
        this->multiply = multiply; this->add = add;
    }
};
