// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "SceneResolution.h"
#include "graphics/Texture.h"

#include <memory>

class CCanvas2D;
class CTextureManager;
namespace Renderer::Backend { class IDevice; class IFramebuffer; class ITexture; }

namespace AndroidPerformance
{
// One reusable color/depth target. The existing canvas shader composites it
// over the complete native-resolution backbuffer before the HUD is drawn.
class SceneBuffer
{
public:
    SceneBuffer();
    ~SceneBuffer();
    Renderer::Backend::IFramebuffer* Prepare(Renderer::Backend::IDevice* device,
        CTextureManager& textureManager, Extent display, int shortEdge);
    void Draw(CCanvas2D& canvas, Extent display) const;
    void Reset();

private:
    Extent m_AttemptedSize;
    CTexturePtr m_Color;
    std::unique_ptr<Renderer::Backend::ITexture> m_Depth;
    // Destroy the framebuffer before the attachments it references.
    std::unique_ptr<Renderer::Backend::IFramebuffer> m_Framebuffer;
};
}
