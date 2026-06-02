#include "renderer.hpp"
#include "../common.hpp"
#include "blit.hpp"

void Renderer::Init(int x, int y) { this->SetRenderResolution(x, y); }

void Renderer::SetRenderResolution(int x, int y) {
  render_res_x = x;
  render_res_y = y;
  bmp.Alloc(render_res_x, render_res_y);
}

void Renderer::LoadPalette(Common const& common) {
  origpal = common.exepal;
  pal = origpal;
}

void Renderer::Clear() { Fill(bmp, 0); }
