// SPDX-License-Identifier: GPL-2.0-or-later
#include "precompiled.h"
#include "AndroidSceneBuffer.h"

#include "graphics/Canvas2D.h"
#include "graphics/Color.h"
#include "graphics/TextureManager.h"
#include "lib/ogl.h"
#include "maths/Rect.h"
#include "ps/CLogger.h"
#include "renderer/backend/Backend.h"
#include "renderer/backend/IDevice.h"
#include "renderer/backend/IFramebuffer.h"
#include "renderer/backend/ITexture.h"
#include "renderer/backend/Sampler.h"

namespace AndroidPerformance
{
SceneBuffer::SceneBuffer() = default;
SceneBuffer::~SceneBuffer() = default;

void SceneBuffer::Reset()
{
    m_Framebuffer.reset();
    m_Depth.reset();
    m_Color.reset();
    m_AttemptedSize = {};
}

Renderer::Backend::IFramebuffer* SceneBuffer::Prepare(
    Renderer::Backend::IDevice* device, CTextureManager& textureManager,
    Extent display, int shortEdge)
{
    using namespace Renderer::Backend;
    const Extent scene = SceneExtent(display, shortEdge);
    if (scene == display || device->GetBackend() != Backend::GL)
    {
        Reset();
        return nullptr;
    }
    if (scene == m_AttemptedSize)
        return m_Framebuffer.get(); // Also avoids retrying an unsupported target every frame.
    Reset();
    m_AttemptedSize = scene;

    if ((!ogl_HaveVersion(3, 0) && !ogl_HaveExtension("GL_OES_depth_texture")) ||
        scene.width > device->GetCapabilities().maxTextureSize ||
        scene.height > device->GetCapabilities().maxTextureSize)
    {
        LOGWARNING("Android scene scaling unavailable; rendering at the full display size.");
        return nullptr;
    }

    auto color = device->CreateTexture2D("AndroidSceneColor",
        ITexture::Usage::COLOR_ATTACHMENT | ITexture::Usage::SAMPLED,
        Format::R8G8B8A8_UNORM, scene.width, scene.height,
        Sampler::MakeDefaultSampler(Sampler::Filter::LINEAR, Sampler::AddressMode::CLAMP_TO_EDGE));
    auto depth = device->CreateTexture2D("AndroidSceneDepth",
        ITexture::Usage::DEPTH_STENCIL_ATTACHMENT,
        Format::D16_UNORM, scene.width, scene.height,
        Sampler::MakeDefaultSampler(Sampler::Filter::NEAREST, Sampler::AddressMode::CLAMP_TO_EDGE));
    if (!color || !depth)
    {
        LOGWARNING("Android scene textures unavailable; rendering at the full display size.");
        return nullptr;
    }

    SColorAttachment colorAttachment{color.get(), AttachmentLoadOp::CLEAR,
        AttachmentStoreOp::STORE, CColor(0, 0, 0, 1)};
    SDepthStencilAttachment depthAttachment{depth.get(), AttachmentLoadOp::CLEAR,
        AttachmentStoreOp::DONT_CARE};
    auto framebuffer = device->CreateFramebuffer("AndroidScene", &colorAttachment, &depthAttachment);
    if (!framebuffer)
    {
        LOGWARNING("Android scene framebuffer unavailable; rendering at the full display size.");
        return nullptr;
    }
    m_Color = textureManager.WrapBackendTexture(std::move(color));
    m_Depth = std::move(depth);
    m_Framebuffer = std::move(framebuffer);
    LOGMESSAGE("Android scene %ux%u, fullscreen display/HUD %ux%u.",
        scene.width, scene.height, display.width, display.height);
    return m_Framebuffer.get();
}

void SceneBuffer::Draw(CCanvas2D& canvas, Extent display) const
{
    // Render targets use a bottom-left origin; canvas destinations use top-left.
    // Force opaque output so scene alpha cannot leave transparent screen edges.
    canvas.DrawTexture(m_Color, CRect(0, 0, display.width, display.height),
        CRect(0, m_Color->GetHeight(), m_Color->GetWidth(), 0),
        CColor(1, 1, 1, 0), CColor(0, 0, 0, 1), 0);
}
}
