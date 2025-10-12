#include "scrollOverview.hpp"
#include <any>
#define private public
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/managers/animation/AnimationManager.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#undef private
#include "OverviewPassElement.hpp"

static void damageMonitor(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview->damage();
}

static void removeOverview(WP<Hyprutils::Animation::CBaseAnimatedVariable> thisptr) {
    g_pOverview.reset();
}

CScrollOverview::~CScrollOverview() {
    g_pHyprRenderer->makeEGLCurrent();
    images.clear(); // otherwise we get a vram leak
    g_pInputManager->unsetCursorImage();
    g_pHyprOpenGL->markBlurDirtyForMonitor(pMonitor.lock());
}

CScrollOverview::CScrollOverview(PHLWORKSPACE startedOn_, bool swipe_) : startedOn(startedOn_), swipe(swipe_) {
    static auto* const* PDEFAULTZOOM = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:scrolling:default_zoom")->getDataStaticPtr();

    const auto          PMONITOR = g_pCompositor->m_lastMonitor.lock();
    pMonitor                     = PMONITOR;

    for (const auto& w : g_pCompositor->getWorkspaces()) {
        if (w && w->m_monitor == pMonitor && !w->m_isSpecialWorkspace)
            images.emplace_back(makeShared<SWorkspaceImage>(w.lock()));
    }

    std::sort(images.begin(), images.end(), [](const auto& a, const auto& b) { return a->pWorkspace->m_id < b->pWorkspace->m_id; });

    g_pAnimationManager->createAnimation(1.F, scale, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);
    g_pAnimationManager->createAnimation({}, viewOffset, g_pConfigManager->getAnimationPropertyConfig("windowsMove"), AVARDAMAGE_NONE);

    scale->setUpdateCallback(damageMonitor);
    viewOffset->setUpdateCallback(damageMonitor);

    if (!swipe)
        *scale = std::clamp(**PDEFAULTZOOM, 0.1F, 0.9F);

    lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

    auto onCursorMove = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled    = true;
        lastMousePosLocal = g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position;

        //  highlightHoverDebug();
    };

    auto onCursorSelect = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled = true;

        selectHoveredWorkspace();

        close();
    };

    auto onMouseAxis = [this](void* self, SCallbackInfo& info, std::any param) {
        if (closing)
            return;

        info.cancelled = true;

        auto                data = std::any_cast<std::unordered_map<std::string, std::any>>(param);
        auto                e    = std::any_cast<IPointer::SAxisEvent>(data["event"]);

        static auto* const* PZOOM = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprexpo:scrolling:scroll_moves_up_down")->getDataStaticPtr();

        if (!**PZOOM) {
            const auto VAL = std::clamp(sc<float>(scale->value() + e.delta / -500.F), 0.05F, 0.95F);
            *scale         = VAL;
        } else
            moveViewportWorkspace(e.delta > 0);
    };

    mouseMoveHook = g_pHookSystem->hookDynamic("mouseMove", onCursorMove);
    touchMoveHook = g_pHookSystem->hookDynamic("touchMove", onCursorMove);
    mouseAxisHook = g_pHookSystem->hookDynamic("mouseAxis", onMouseAxis);

    mouseButtonHook = g_pHookSystem->hookDynamic("mouseButton", onCursorSelect);
    touchDownHook   = g_pHookSystem->hookDynamic("touchDown", onCursorSelect);

    g_pInputManager->setCursorImageUntilUnset("left_ptr");

    redrawAll();

    size_t activeIdx = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i]->pWorkspace && images[i]->pWorkspace == startedOn) {
            activeIdx = i;
            break;
        }
    }

    viewportCurrentWorkspace = activeIdx;
}

void CScrollOverview::selectHoveredWorkspace() {
    size_t activeIdx = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i]->pWorkspace && images[i]->pWorkspace == startedOn) {
            activeIdx = i;
            break;
        }
    }

    const auto VIEWPORT_CENTER = CBox{{}, pMonitor->m_size}.middle();

    float      yoff  = -(float)activeIdx * pMonitor->m_size.y * scale->value();
    bool       found = false;
    for (const auto& wimg : images) {
        for (const auto& img : wimg->windowImages) {
            CBox texbox = {img->pWindow->m_realPosition->value() - pMonitor->m_position, img->pWindow->m_realSize->value()};

            // scale the box to the viewport center
            texbox.translate(-VIEWPORT_CENTER).scale(scale->value()).translate(VIEWPORT_CENTER).translate(-viewOffset->value() * scale->value());

            texbox.translate({0.F, yoff});

            // texbox.scale(pMonitor->m_scale).round();

            if (texbox.containsPoint(lastMousePosLocal)) {
                closeOnWindow = img->pWindow;
                // *viewOffset   = CBox{img->pWindow->m_realPosition->value(), img->pWindow->m_realSize->value()}.translate({0.F, yoff / scale->value()}).middle() -
                //     CBox{pMonitor->m_position, pMonitor->m_size}.middle();
                found = true;
                break;
            }
        }
        if (found)
            break;
        yoff += pMonitor->m_size.y * scale->value();
    }
}

void CScrollOverview::moveViewportWorkspace(bool up) {
    size_t activeIdx = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i]->pWorkspace && images[i]->pWorkspace == startedOn) {
            activeIdx = i;
            break;
        }
    }

    if (viewportCurrentWorkspace == 0 && !up)
        return;
    if (viewportCurrentWorkspace == images.size() - 1 && up)
        return;

    if (up)
        viewportCurrentWorkspace++;
    else
        viewportCurrentWorkspace--;

    *viewOffset = {viewOffset->value().x, (sc<long>(viewportCurrentWorkspace) - sc<long>(activeIdx)) * pMonitor->m_size.y};
}

void CScrollOverview::highlightHoverDebug() {
    size_t activeIdx = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i]->pWorkspace && images[i]->pWorkspace == startedOn) {
            activeIdx = i;
            break;
        }
    }

    const auto VIEWPORT_CENTER = CBox{{}, pMonitor->m_size}.middle();

    float      yoff = -(float)activeIdx * pMonitor->m_size.y * scale->value();
    for (const auto& wimg : images) {
        for (const auto& img : wimg->windowImages) {
            CBox texbox = {img->pWindow->m_realPosition->value() - pMonitor->m_position, img->pWindow->m_realSize->value()};

            // scale the box to the viewport center
            texbox.translate(-VIEWPORT_CENTER).scale(scale->value()).translate(VIEWPORT_CENTER).translate(-viewOffset->value() * scale->value());

            texbox.translate({0.F, yoff});

            // texbox.scale(pMonitor->m_scale).round();

            if (texbox.containsPoint(lastMousePosLocal)) {
                img->highlight = true;
                continue;
            }

            img->highlight = false;
        }
        yoff += pMonitor->m_size.y * scale->value();
    }
}

SP<CScrollOverview::SWorkspaceImage> CScrollOverview::imageForWorkspace(PHLWORKSPACE w) {
    for (const auto& i : images) {
        if (i->pWorkspace == w)
            return i;
    }
    return nullptr;
}

void CScrollOverview::redrawWorkspace(PHLWORKSPACE workspace, bool forcelowres) {
    if (pMonitor->m_activeWorkspace != startedOn && !closing) {
        // likely user changed.
        onWorkspaceChange();
    }

    blockOverviewRendering = true;

    g_pHyprRenderer->makeEGLCurrent();

    auto image = imageForWorkspace(workspace);

    if (!image)
        return;

    // get all tiled windows on the workspace and max dim
    // TODO: float
    std::vector<PHLWINDOW> windows;
    for (const auto& w : g_pCompositor->m_windows) {
        if (!validMapped(w) || w->m_isFloating || w->m_workspace != workspace)
            continue;
        windows.emplace_back(w);
    }

    for (const auto& w : windows) {
        auto img     = image->windowImages.emplace_back(makeShared<SWindowImage>());
        img->pWindow = w;
        img->fb.alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, pMonitor->m_output->state->state().drmFormat);
        if (!w->m_isX11 && w->m_wlSurface) {
            img->windowCommit = makeUnique<CHyprSignalListener>(w->m_wlSurface->resource()->m_events.commit.listen([wk = WP<SWindowImage>{img}] {
                if (!wk || !wk->pWindow)
                    return;

                if (wk->pWindow->m_wlSurface->resource()->m_current.accumulateBufferDamage().empty())
                    return;

                reinterpretPointerCast<CScrollOverview>(g_pOverview)->redrawWindowImage(wk.lock());
                g_pOverview->damage();
            }));
        }

        redrawWindowImage(img);
    }

    blockOverviewRendering = false;
}

void CScrollOverview::redrawWindowImage(SP<SWindowImage> img) {
    if (!img->pWindow)
        return;

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &img->fb);

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 0});

    g_pHyprRenderer->renderWindow(img->pWindow.lock(), pMonitor.lock(), Time::steadyNow(), true, RENDER_PASS_ALL, true, true);

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();
}

void CScrollOverview::redrawAll(bool forcelowres) {

    for (const auto& img : images) {
        redrawWorkspace(img->pWorkspace);
    }

    // redraw bg
    if (backgroundFb.m_size != pMonitor->m_pixelSize) {
        backgroundFb.release();
        backgroundFb.alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, pMonitor->m_output->state->state().drmFormat);
        floatingFb.alloc(pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y, pMonitor->m_output->state->state().drmFormat);
    }

    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &backgroundFb);

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1.0});

    g_pHyprRenderer->renderAllClientsForWorkspace(pMonitor.lock(), nullptr, Time::steadyNow());

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    // render floating as well. For these, we disable decos to match tiled ones.
    g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &floatingFb);

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 0});

    for (const auto& w : g_pCompositor->m_windows) {
        if (!validMapped(w) || !w->m_isFloating || w->m_workspace != startedOn)
            continue;

        g_pHyprRenderer->renderWindow(w, pMonitor.lock(), Time::steadyNow(), false, RENDER_PASS_ALL);
    }

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();
}

void CScrollOverview::damage() {
    blockDamageReporting = true;
    g_pHyprRenderer->damageMonitor(pMonitor.lock());
    blockDamageReporting = false;
}

void CScrollOverview::onDamageReported() {
    ; // TODO:
}

void CScrollOverview::close() {

    if (closing)
        return;

    closing = true;

    if (!closeOnWindow)
        closeOnWindow = g_pCompositor->m_lastWindow;

    if (closeOnWindow == g_pCompositor->m_lastWindow)
        *viewOffset = Vector2D{};
    else {

        if (closeOnWindow->m_workspace != pMonitor->m_activeWorkspace) {
            g_pDesktopAnimationManager->startAnimation(pMonitor->m_activeWorkspace, CDesktopAnimationManager::ANIMATION_TYPE_OUT, true, true);
            g_pDesktopAnimationManager->startAnimation(closeOnWindow->m_workspace, CDesktopAnimationManager::ANIMATION_TYPE_IN, false, true);
            pMonitor->changeWorkspace(closeOnWindow->m_workspace, true, true, true);
        }

        g_pCompositor->focusWindow(closeOnWindow.lock());

        size_t activeIdx = 0;
        for (size_t i = 0; i < images.size(); ++i) {
            if (images[i]->pWorkspace && images[i]->pWorkspace == startedOn) {
                activeIdx = i;
                break;
            }
        }

        float yoff  = -(float)activeIdx * pMonitor->m_size.y * scale->value();
        bool  found = false;
        for (const auto& wimg : images) {
            for (const auto& img : wimg->windowImages) {
                if (img->pWindow == closeOnWindow && closeOnWindow) {
                    Vector2D middleOfWindow = CBox{img->pWindow->m_realPosition->value(), img->pWindow->m_realSize->value()}.translate({0.F, yoff / scale->value()}).middle() -
                        CBox{pMonitor->m_position, pMonitor->m_size}.middle();

                    // we need to do this because the window doesnt have to be centered after click
                    *viewOffset = middleOfWindow +
                        (CBox{pMonitor->m_position, pMonitor->m_size}.middle() - CBox{img->pWindow->m_realPosition->value(), img->pWindow->m_realSize->value()}.middle());
                    found = true;
                    break;
                }
            }
            if (found)
                break;
            yoff += pMonitor->m_size.y * scale->value();
        }
    }

    *scale = 1.F;

    scale->setCallbackOnEnd(removeOverview);
}

void CScrollOverview::onPreRender() {
    ;
}

void CScrollOverview::onWorkspaceChange() {
    ; // TODO:
}

void CScrollOverview::render() {
    g_pHyprRenderer->m_renderPass.add(makeUnique<COverviewPassElement>());
}

void CScrollOverview::fullRender() {

    g_pHyprOpenGL->clear(CHyprColor{0, 0, 0, 1});

    CBox texbox = {{}, pMonitor->m_size};
    texbox.scale(pMonitor->m_scale);
    texbox.round();
    CRegion damage{0, 0, INT16_MAX, INT16_MAX};
    g_pHyprOpenGL->renderTextureInternal(backgroundFb.getTexture(), texbox, {.damage = &damage, .a = 1.0});

    const auto VIEWPORT_CENTER = CBox{{}, pMonitor->m_size}.middle();

    size_t     activeIdx = 0;
    for (size_t i = 0; i < images.size(); ++i) {
        if (images[i]->pWorkspace && images[i]->pWorkspace == startedOn) {
            activeIdx = i;
            break;
        }
    }

    // render all views
    float yoff = -(float)activeIdx * pMonitor->m_size.y * scale->value();
    for (const auto& wimg : images) {
        for (const auto& img : wimg->windowImages) {
            CBox texbox = CBox{img->pWindow->m_realPosition->value() - pMonitor->m_position, pMonitor->m_size};

            // scale the box to the viewport center
            texbox.translate(-VIEWPORT_CENTER).scale(scale->value()).translate(VIEWPORT_CENTER).translate(-viewOffset->value() * scale->value());

            texbox.translate({0.F, yoff});

            texbox.scale(pMonitor->m_scale).round();
            CRegion damage{0, 0, INT16_MAX, INT16_MAX};
            g_pHyprOpenGL->renderTextureInternal(img->fb.getTexture(), texbox, {.damage = &damage, .a = 1.0});

            if (img->highlight) {
                CBox texbox2 = CBox{img->pWindow->m_realPosition->value(), img->pWindow->m_realSize->value()}
                                   .translate(-VIEWPORT_CENTER)
                                   .scale(scale->value())
                                   .translate(VIEWPORT_CENTER)
                                   .translate({0.F, yoff});
                g_pHyprOpenGL->renderRect(texbox2, CHyprColor{0.5, 0.0, 0.0, 0.5}, CHyprOpenGLImpl::SRectRenderData{.round = 5});
            }
        }
        CBox floatbox = CBox{pMonitor->m_position + Vector2D{0.F, yoff / scale->value()}, pMonitor->m_size};
        floatbox.translate(-VIEWPORT_CENTER).scale(scale->value()).translate(VIEWPORT_CENTER).translate(-viewOffset->value() * scale->value());
        floatbox.translate({0.F, yoff});
        floatbox.scale(pMonitor->m_scale).round();
        g_pHyprOpenGL->renderTextureInternal(floatingFb.getTexture(), floatbox, {.damage = &damage, .a = 1.0});

        yoff += pMonitor->m_size.y * scale->value();
    }
}

static float lerp(const float& from, const float& to, const float perc) {
    return (to - from) * perc + from;
}

static Vector2D lerp(const Vector2D& from, const Vector2D& to, const float perc) {
    return Vector2D{lerp(from.x, to.x, perc), lerp(from.y, to.y, perc)};
}

void CScrollOverview::setClosing(bool closing_) {
    // TODO:
}

void CScrollOverview::resetSwipe() {
    // TODO:
}

void CScrollOverview::onSwipeUpdate(double delta) {
    // TODO:
}

void CScrollOverview::onSwipeEnd() {
    // TODO:
}
