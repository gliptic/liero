#include "renderer.hpp"
#include "../common.hpp"
#include "blit.hpp"

void Renderer::Init(int x, int y) { this->SetRenderResolution(x, y); }

void Renderer::SetRenderResolution(int x, int y) {
  render_res_x = x;
  render_res_y = y;
  bmp.Alloc(render_res_x, render_res_y);
  bmp.pal32 = pal32;
}

void Renderer::LoadPalette(Common const& common) {
  origpal = common.exepal;
  origpal_modern = common.modernpal;
  pal = Origpal();
  UpdatePal32();
}

void Renderer::Clear() { Fill(bmp, 0); }

void Renderer::UpdatePal32() {
  for (int i = 0; i < 256; ++i) {
    Color const& e = pal.entries[i];
    pal32[i] =
        0xFF000000U | (static_cast<uint32_t>(e.r) << 16) | (static_cast<uint32_t>(e.g) << 8) | e.b;
  }
}
