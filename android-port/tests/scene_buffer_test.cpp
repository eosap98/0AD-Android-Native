// SPDX-License-Identifier: GPL-2.0-or-later
#include "AndroidSceneBuffer.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

using AndroidPerformance::Extent;
using AndroidPerformance::SceneExtent;
using AndroidPerformance::SceneBuffer;
using Renderer::Backend::IDevice;

static void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    int passed = 0;
    auto test = [&](const char* name, const std::function<void()>& run) {
        SceneTest::depthTexture = true; SceneTest::coreDepth = false; SceneTest::warnings = 0;
        run();
        Require(SceneTest::textures == 0 && SceneTest::framebuffers == 0, "Leaked framebuffer attachments");
        ++passed;
        std::cout << "PASS " << name << '\n';
    };
    test("3D reduction leaves the Redmi fullscreen extent unchanged", [] {
        const Extent display{2400, 1080};
        Require(SceneExtent(display, 720) == Extent{1600, 720}, "Wrong scene dimensions");
        Require(display == Extent{2400, 1080}, "Display was resized");
        Require(SceneExtent({1920, 1080}, 720) == Extent{1280, 720}, "16:9 scene");
        Require(SceneExtent({1080, 2400}, 720) == Extent{720, 1600}, "Portrait scene");
    });
    test("small displays and disabled scaling use native resolution", [] {
        Require(SceneExtent({960, 540}, 720) == Extent{960, 540}, "Upscaled small display");
        Require(SceneExtent({1281, 719}, 720) == Extent{1281, 719}, "Changed native odd dimensions");
        for (int edge : {0, -1, 2160})
            Require(SceneExtent({2400, 1080}, edge) == Extent{2400, 1080}, "Scaling not disabled");
    });
    test("odd scene sizes remain bounded and preserve aspect ratio", [] {
        const Extent display{2401, 1081};
        const Extent scene = SceneExtent(display, 720);
        Require(scene.width % 2 == 0 && scene.height % 2 == 0, "Unaligned offscreen target");
        Require(scene.width <= display.width && scene.height <= display.height, "Upscaling");
        Require(std::abs(double(scene.width) - display.width * (double(scene.height) / display.height)) < 2, "Distorted aspect ratio");
        Require(SceneExtent({0, 1080}, 720) == Extent{0, 1080}, "Invalid display changed");
    });
    test("color and depth targets are reused across frames", [] {
        IDevice device; CTextureManager textures; SceneBuffer buffer;
        auto* framebuffer = buffer.Prepare(&device, textures, {2400, 1080}, 720);
        Require(framebuffer && framebuffer->GetWidth() == 1600 && framebuffer->GetHeight() == 720, "Missing target");
        for (int i = 0; i < 120; ++i)
            Require(buffer.Prepare(&device, textures, {2400, 1080}, 720) == framebuffer, "Target recreated each frame");
        Require(device.textureCalls == 2 && device.framebufferCalls == 1, "Unexpected allocations");
        Require(device.filters[0] == Renderer::Backend::Sampler::Filter::LINEAR, "Upscale must filter linearly");
    });
    test("scene composition covers all display edges with correct orientation and opacity", [] {
        IDevice device; CTextureManager textures; SceneBuffer buffer; CCanvas2D canvas;
        buffer.Prepare(&device, textures, {2400, 1080}, 720);
        buffer.Draw(canvas, {2400, 1080});
        Require(canvas.draws == 1, "Unexpected composition passes");
        Require(canvas.destination.left == 0 && canvas.destination.top == 0 &&
            canvas.destination.right == 2400 && canvas.destination.bottom == 1080, "Uncovered fullscreen edges");
        Require(canvas.source.left == 0 && canvas.source.right == 1600 &&
            canvas.source.top == 720 && canvas.source.bottom == 0, "Render target vertically flipped");
        Require(canvas.multiply.a == 0 && canvas.add.a == 1, "Scene alpha can leave screen holes");
    });
    test("display resizing recreates attachments in safe lifetime order", [] {
        IDevice device; CTextureManager textures; SceneBuffer buffer;
        buffer.Prepare(&device, textures, {2400, 1080}, 720);
        auto* framebuffer = buffer.Prepare(&device, textures, {1920, 1080}, 720);
        Require(framebuffer && framebuffer->GetWidth() == 1280, "Resize ignored");
        Require(device.textureCalls == 4 && SceneTest::textures == 2 && SceneTest::framebuffers == 1, "Old targets retained");
    });
    test("unsupported depth textures fall back without repeated allocation attempts", [] {
        SceneTest::depthTexture = false;
        IDevice device; CTextureManager textures; SceneBuffer buffer;
        for (int i = 0; i < 120; ++i)
            Require(!buffer.Prepare(&device, textures, {2400, 1080}, 720), "Unsupported target accepted");
        Require(device.textureCalls == 0 && SceneTest::warnings == 1, "Repeated fallback work");
    });
    test("GLES core depth support does not require an extension string", [] {
        SceneTest::depthTexture = false; SceneTest::coreDepth = true;
        IDevice device; CTextureManager textures; SceneBuffer buffer;
        Require(buffer.Prepare(&device, textures, {2400, 1080}, 720), "Core support ignored");
    });
    test("texture allocation failure releases partial resources and falls back", [] {
        IDevice device; device.failTextureCall = 2;
        CTextureManager textures; SceneBuffer buffer;
        for (int i = 0; i < 10; ++i)
            Require(!buffer.Prepare(&device, textures, {2400, 1080}, 720), "Failed texture accepted");
        Require(device.textureCalls == 2 && SceneTest::textures == 0, "Partial target leak or retry loop");
    });
    test("incomplete framebuffer falls back without a per-frame retry", [] {
        IDevice device; device.failFramebuffer = true;
        CTextureManager textures; SceneBuffer buffer;
        for (int i = 0; i < 10; ++i)
            Require(!buffer.Prepare(&device, textures, {2400, 1080}, 720), "Failed framebuffer accepted");
        Require(device.framebufferCalls == 1 && SceneTest::textures == 0, "Framebuffer retry loop or leak");
    });
    test("texture-size limits cannot allocate an oversized target", [] {
        IDevice device; device.capabilities.maxTextureSize = 1024;
        CTextureManager textures; SceneBuffer buffer;
        Require(!buffer.Prepare(&device, textures, {2400, 1080}, 720), "Oversized target");
        Require(device.textureCalls == 0, "Allocated beyond device limits");
    });
    test("turning off scaling releases targets and can be reversed", [] {
        IDevice device; CTextureManager textures; SceneBuffer buffer;
        buffer.Prepare(&device, textures, {2400, 1080}, 720);
        Require(!buffer.Prepare(&device, textures, {2400, 1080}, 0), "Scaling still enabled");
        Require(SceneTest::textures == 0 && SceneTest::framebuffers == 0, "Disabled targets retained");
        Require(buffer.Prepare(&device, textures, {2400, 1080}, 720), "Scaling did not recover");
    });
    test("a resize can recover from a previous allocation failure", [] {
        IDevice device; device.failFramebuffer = true;
        CTextureManager textures; SceneBuffer buffer;
        buffer.Prepare(&device, textures, {2400, 1080}, 720);
        device.failFramebuffer = false;
        Require(buffer.Prepare(&device, textures, {1920, 1080}, 720), "Failed target never retried after resize");
    });
    test("other renderer backends retain their native path", [] {
        IDevice device; device.backend = Renderer::Backend::Backend::DUMMY;
        CTextureManager textures; SceneBuffer buffer;
        Require(!buffer.Prepare(&device, textures, {2400, 1080}, 720), "Other backend intercepted");
        Require(device.textureCalls == 0, "Allocated on another backend");
    });
    std::cout << passed << " scene-buffer checks passed; renderer services are fixtures, no emulator used.\n";
}
