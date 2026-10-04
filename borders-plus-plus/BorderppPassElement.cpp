#include "BorderppPassElement.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include "borderDeco.hpp"

CBorderPPPassElement::CBorderPPPassElement(const CBorderPPPassElement::SBorderPPData& data_) : data(data_) {
    ;
}

std::vector<UP<IPassElement>> CBorderPPPassElement::draw(Render::CRenderContext& ctx) {
    data.deco->drawPass(ctx, ctx.m_data.pMonitor.lock(), data.a);
    return {};
}

bool CBorderPPPassElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CBorderPPPassElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return false;
}
