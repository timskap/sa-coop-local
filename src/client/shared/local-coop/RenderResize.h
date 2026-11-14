#pragma once
#include <cstdint>

namespace coop::local {
struct RenderSize {
    int width = 0, height = 0;
    bool Valid() const { return width > 0 && height > 0; }
    bool operator==(const RenderSize&) const = default;
};
struct RenderRaster {
    uintptr_t id = 0;
    RenderSize size;
    bool operator==(const RenderRaster&) const = default;
    bool Covers(RenderSize view) const {
        return id && view.Valid() && size.width >= view.width && size.height >= view.height;
    }
};

// Native WM_SIZE changes the camera before RenderWare resets the D3D backbuffer.
// Refresh post-effects only once both agree, before the next frame is copied into
// the effect raster. Exact viewport size matters even within one power-of-two
// texture allocation: UVs, filter vertices and heat-haze bounds must change too.
class PostFxResize {
    uintptr_t camera = 0;
    RenderRaster frame{}, front{};
public:
    void Invalidate() { camera = 0; frame = front = {}; }
    template<class Refresh>
    bool Prepare(uintptr_t nextCamera, RenderRaster nextFrame, RenderSize backBuffer,
                 RenderRaster nextFront, Refresh refresh) {
        if (!nextCamera || !nextFrame.id || !nextFrame.size.Valid() || nextFrame.size != backBuffer) {
            Invalidate();
            return false;
        }
        if (camera == nextCamera && frame == nextFrame && front == nextFront && nextFront.Covers(nextFrame.size)) return true;
        nextFront = refresh();
        if (!nextFront.Covers(nextFrame.size)) {
            Invalidate();
            return false;
        }
        camera = nextCamera;
        frame = nextFrame;
        front = nextFront;
        return true;
    }
};
}  // namespace coop::local
