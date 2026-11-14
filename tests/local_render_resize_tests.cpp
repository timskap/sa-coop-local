#include <cassert>
#include <bit>
#include <iostream>
#include "shared/local-coop/RenderResize.h"

int main() {
    using namespace coop::local;
    PostFxResize resize;
    RenderRaster frame{1, {1920, 540}}, front{2, {2048, 1024}};
    unsigned rebuilds = 0;
    RenderSize filterView{};
    bool allocationFails = false;
    auto rebuild = [&] {
        ++rebuilds;
        filterView = frame.size;
        if (allocationFails) return RenderRaster{};
        front.size = {static_cast<int>(std::bit_ceil(static_cast<unsigned>(frame.size.width))),
                      static_cast<int>(std::bit_ceil(static_cast<unsigned>(frame.size.height)))};
        return front;
    };
    auto prepare = [&](RenderSize actual) { return resize.Prepare(10, frame, actual, front, rebuild); };
    assert(!prepare({}) && rebuilds == 0); // lost/uncreated D3D backbuffer
    assert(prepare(frame.size) && rebuilds == 1 && filterView == frame.size);
    for (int i = 0; i < 200; ++i) assert(prepare(frame.size));
    assert(rebuilds == 1); // no per-frame resource churn

    // Reproduce the screenshot's transition: WM_SIZE has expanded the camera,
    // but the old half-screen effect texture cannot receive that frame yet.
    frame.size = {1920, 1080};
    assert(!front.Covers(frame.size));
    assert(!prepare({1920, 540}) && rebuilds == 1); // wait for native device reset
    assert(prepare(frame.size) && rebuilds == 2 && front.Covers(frame.size));
    assert(filterView == frame.size); // filter coordinates updated before copying the new scene

    // Return from the scene; even the larger texture's UV mapping is now wrong.
    frame.size = {1920, 540};
    assert(front.Covers(frame.size));
    assert(prepare(frame.size) && rebuilds == 3 && filterView == frame.size);
    const RenderSize capacity = front.size;
    frame.size = {1919, 539};
    assert(prepare(frame.size) && rebuilds == 4 && filterView == frame.size && front.size == capacity);

    // A failed allocation must not become a successful cached resize.
    frame.size = {960, 1080}; // vertical split also changes both raster axes
    allocationFails = true;
    assert(!prepare(frame.size) && rebuilds == 5);
    allocationFails = false;
    assert(prepare(frame.size) && rebuilds == 6);
    front.id = 20; // engine recreated effects while keeping the same viewport
    assert(prepare(frame.size) && rebuilds == 7);
    frame.id = 30; // camera raster recreated on world load
    assert(prepare(frame.size) && rebuilds == 8);
    assert(resize.Prepare(11, frame, frame.size, front, rebuild) && rebuilds == 9);
    assert(!resize.Prepare(0, frame, frame.size, front, rebuild) && rebuilds == 9);
    assert(prepare(frame.size) && rebuilds == 10);

    // Transient reset with identical final dimensions still invalidates old resources.
    assert(!prepare({}) && rebuilds == 10);
    assert(prepare(frame.size) && rebuilds == 11);
    resize.Invalidate();
    assert(!resize.Prepare(10, frame, frame.size, front, [] { return RenderRaster{2, {100, 100}}; }));
    assert(prepare(frame.size) && rebuilds == 12);
    frame.size.width = 0;
    assert(!prepare(frame.size) && rebuilds == 12);
    std::cout << "Local render resize: split/full/vertical transitions, same-capacity UV refresh, delayed reset and allocation recovery passed\n";
}
