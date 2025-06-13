#include "trail.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/render/Renderer.hpp>

#include "globals.hpp"
#include "TrailPassElement.hpp"

void CTrail::onTick() {
    static auto* const PHISTORYSTEP   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:history_step")->getDataStaticPtr();
    static auto* const PHISTORYPOINTS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:history_points")->getDataStaticPtr();

    m_iTimer++;

    const auto PWINDOW = m_pWindow.lock();

    if (m_iTimer > **PHISTORYSTEP) {
        m_dLastGeoms.push_front({box{(float)PWINDOW->m_realPosition->value().x, (float)PWINDOW->m_realPosition->value().y, (float)PWINDOW->m_realSize->value().x,
                                     (float)PWINDOW->m_realSize->value().y},
                                 std::chrono::system_clock::now()});
        while (m_dLastGeoms.size() > **PHISTORYPOINTS)
            m_dLastGeoms.pop_back();

        m_iTimer = 0;
    }

    if (m_bNeedsDamage) {
        g_pHyprRenderer->damageBox(m_bLastBox);
        m_bNeedsDamage = false;
    }
}

CTrail::CTrail(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    m_lastWindowPos  = pWindow->m_realPosition->value();
    m_lastWindowSize = pWindow->m_realSize->value();

    pTickCb = HyprlandAPI::registerCallbackDynamic(PHANDLE, "trailTick", [this](void* self, SCallbackInfo& info, std::any data) { this->onTick(); });
}

CTrail::~CTrail() {
    damageEntire();
    HyprlandAPI::unregisterCallback(PHANDLE, pTickCb);
}

SDecorationPositioningInfo CTrail::getPositioningInfo() {
    return {DECORATION_POSITION_ABSOLUTE};
}

void CTrail::onPositioningReply(const SDecorationPositioningReply& reply) {
    ; // ignored
}

void scaleBox2(box& box, float coeff) {
    float hwl = (box.w - (box.w * coeff)) / 2.0;
    float hhl = (box.h - (box.h * coeff)) / 2.0;

    box.w *= coeff;
    box.h *= coeff;
    box.x += hwl;
    box.y += hhl;
}

Vector2D vecForT(const Vector2D& a, const Vector2D& b, const float& t) {
    const Vector2D vec_PQ = b - a;
    return Vector2D{a + vec_PQ * t};
}

Vector2D vecForBezierT(const float& t, const std::vector<Vector2D>& verts) {
    std::vector<Vector2D> pts;

    for (size_t vertexIndex = 0; vertexIndex < verts.size() - 1; vertexIndex++) {
        Vector2D p = verts[vertexIndex];
        pts.push_back(vecForT(p, verts[vertexIndex + 1], t));
    }

    if (pts.size() > 1)
        return vecForBezierT(t, pts);
    else
        return pts[0];
}

void CTrail::draw(PHLMONITOR pMonitor, const float& a) {
    if (!validMapped(m_pWindow))
        return;

    const auto PWINDOW = m_pWindow.lock();

    if (!PWINDOW->m_windowData.decorate.valueOrDefault())
        return;

    auto data = CTrailPassElement::STrailData{this, a};
    g_pHyprRenderer->m_renderPass.add(makeShared<CTrailPassElement>(data));
}

void CTrail::renderPass(PHLMONITOR pMonitor, const float& a) {
    const auto PWINDOW = m_pWindow.lock();

    static auto* const PBEZIERSTEP    = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:bezier_step")->getDataStaticPtr();
    static auto* const PPOINTSPERSTEP = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:points_per_step")->getDataStaticPtr();
    static auto* const PCOLOR         = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:color")->getDataStaticPtr();

    const CHyprColor   COLOR = **PCOLOR;

    if (m_dLastGeoms.size() < 2)
        return;

    box thisbox =
        box{(float)PWINDOW->m_realPosition->value().x, (float)PWINDOW->m_realPosition->value().y, (float)PWINDOW->m_realSize->value().x, (float)PWINDOW->m_realSize->value().y};
    CBox wlrbox = {thisbox.x - pMonitor->m_position.x, thisbox.y - pMonitor->m_position.y, thisbox.w, thisbox.h};
    wlrbox.scale(pMonitor->m_scale).round();

    g_pHyprOpenGL->scissor(nullptr); // allow the entire window and stencil to render
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    g_pHyprOpenGL->renderRect(wlrbox, CHyprColor(0, 0, 0, 0), PWINDOW->rounding() * pMonitor->m_scale, PWINDOW->roundingPower());
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_NOTEQUAL, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    CBox   monbox = {0, 0, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.x, g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.y};

    Mat3x3 matrix   = g_pHyprOpenGL->m_renderData.monitorProjection.projectBox(monbox, wlTransformToHyprutils(invertTransform(WL_OUTPUT_TRANSFORM_NORMAL)), monbox.rot);
    Mat3x3 glMatrix = g_pHyprOpenGL->m_renderData.projection.copy().multiply(matrix);

    g_pHyprOpenGL->blend(true);

    glUseProgram(g_pGlobalState->trailShader.program);

#ifndef GLES2
    glUniformMatrix3fv(g_pGlobalState->trailShader.proj, 1, GL_TRUE, glMatrix.getMatrix().data());
#else
    glMatrix.transpose();
    glUniformMatrix3fv(g_pGlobalState->trailShader.proj, 1, GL_FALSE, glMatrix.getMatrix().data());
#endif

    std::vector<point2>   points;
    std::vector<Vector2D> bezierPts;
    std::vector<Vector2D> pointsForBezier;
    std::vector<float>    agesForBezier;

    float                 originalCoeff = 50;

    auto                  msFrom = [](std::chrono::system_clock::time_point t) -> float {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - t).count() / 1000.0;
    };

    auto dist = [&](const point2& a, const point2& b) -> float {
        Vector2D diff = Vector2D{a.x - b.x, a.y - b.y} * pMonitor->m_size;
        return std::sqrt(diff.x * diff.x + diff.y * diff.y);
    };

    float    msMaxToLast = msFrom(m_dLastGeoms.back().second);

    float    dists[2] = {0, 0};

    Vector2D mainVec      = {originalCoeff / pMonitor->m_size.x, originalCoeff / pMonitor->m_size.y};
    Vector2D windowMiddle = PWINDOW->middle() - pMonitor->m_position;

    points.push_back(
        Vector2D{cos(0) * mainVec.x - sin(0) * mainVec.y + windowMiddle.x / pMonitor->m_size.x, sin(0) * mainVec.x + cos(0) * mainVec.y + windowMiddle.y / pMonitor->m_size.y});
    points.push_back(Vector2D{cos(-M_PI_2) * mainVec.x - sin(-M_PI_2) * mainVec.y + windowMiddle.x / pMonitor->m_size.x,
                              sin(-M_PI_2) * mainVec.x + cos(-M_PI_2) * mainVec.y + windowMiddle.y / pMonitor->m_size.y});
    points.push_back(Vector2D{cos(M_PI_2) * mainVec.x - sin(M_PI_2) * mainVec.y + windowMiddle.x / pMonitor->m_size.x,
                              sin(M_PI_2) * mainVec.x + cos(M_PI_2) * mainVec.y + windowMiddle.y / pMonitor->m_size.y});
    points.push_back(Vector2D{cos(M_PI) * mainVec.x - sin(M_PI) * mainVec.y + windowMiddle.x / pMonitor->m_size.x,
                              sin(M_PI) * mainVec.x + cos(M_PI) * mainVec.y + windowMiddle.y / pMonitor->m_size.y});

    pointsForBezier.push_back(windowMiddle);
    agesForBezier.push_back(0);

    for (size_t i = 0; i < m_dLastGeoms.size(); i += 1) {
        box box = m_dLastGeoms[i].first;
        box.x -= pMonitor->m_position.x;
        box.y -= pMonitor->m_position.y;
        Vector2D middle = {box.x + box.w / 2.0, box.y + box.h / 2.0};

        if (middle == pointsForBezier[pointsForBezier.size() - 1])
            continue;

        pointsForBezier.push_back(middle);
        agesForBezier.push_back(msFrom(m_dLastGeoms[i].second));
    }

    if (pointsForBezier.size() < 3) {
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glDisable(GL_STENCIL_TEST);

        glStencilMask(-1);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        g_pHyprOpenGL->scissor(nullptr);
        return;
    }

    float maxAge          = agesForBezier.back();
    float tCoeff          = **PBEZIERSTEP;
    int   pointsPerBezier = **PPOINTSPERSTEP;
    bezierPts.push_back(vecForBezierT(0, pointsForBezier));
    for (float t = tCoeff; t <= 1.0; t += tCoeff) {
        bezierPts.push_back(vecForBezierT(t, pointsForBezier));

        const Vector2D& lastbezier     = bezierPts.back();
        const Vector2D& lastprevbezier = bezierPts[bezierPts.size() - 2];

        for (size_t i = 1; i < pointsPerBezier + 1; ++i) {
            const float     bezierPointStep = (1.0 / (pointsPerBezier + 2));
            const Vector2D& middle          = vecForT(lastprevbezier, lastbezier, bezierPointStep * (i + 1));
            const Vector2D& lastmiddle      = vecForT(lastprevbezier, lastbezier, bezierPointStep * i);

            Vector2D        vecNormal = {middle.x - lastmiddle.x, middle.y - lastmiddle.y};

            // normalize vec
            float invlen = 1.0 / std::sqrt(vecNormal.x * vecNormal.x + vecNormal.y * vecNormal.y);
            vecNormal.x *= invlen;
            vecNormal.y *= invlen;

            // make sure it points up
            // vecNormal.y = std::abs(vecNormal.y);

            // extend by coeff
            float ageCoeff = t * (agesForBezier.size() - 1);
            float ageFloor = std::floor(ageCoeff);
            float ageCeil  = std::ceil(ageCoeff);
            float approxAge =
                agesForBezier[static_cast<int>(ageFloor)] + (agesForBezier[static_cast<int>(ageCeil)] - agesForBezier[static_cast<int>(ageFloor)]) * (ageCoeff - ageFloor);
            float    coeff  = originalCoeff * (1.0 - (approxAge / maxAge));
            Vector2D newVec = {vecNormal.x * coeff / pMonitor->m_size.x, vecNormal.y * coeff / pMonitor->m_size.y};

            if ((newVec.x == 0 && newVec.y == 0) || std::isnan(newVec.x) || std::isnan(newVec.y))
                continue;

            // rotate by 90 and -90 and add middle
            points.push_back(Vector2D{cos(M_PI_2) * newVec.x - sin(M_PI_2) * newVec.y + middle.x / pMonitor->m_size.x,
                                      sin(M_PI_2) * newVec.x + cos(M_PI_2) * newVec.y + middle.y / pMonitor->m_size.y});
            points.push_back(Vector2D{cos(-M_PI_2) * newVec.x - sin(-M_PI_2) * newVec.y + middle.x / pMonitor->m_size.x,
                                      sin(-M_PI_2) * newVec.x + cos(-M_PI_2) * newVec.y + middle.y / pMonitor->m_size.y});
        }
    }

    box thisboxopengl = box{(PWINDOW->m_realPosition->value().x - pMonitor->m_position.x) / pMonitor->m_size.x,
                            (PWINDOW->m_realPosition->value().y - pMonitor->m_position.y) / pMonitor->m_size.y,
                            (PWINDOW->m_realPosition->value().x + PWINDOW->m_realSize->value().x) / pMonitor->m_size.x,
                            (PWINDOW->m_realPosition->value().y + PWINDOW->m_realSize->value().y) / pMonitor->m_size.y};
    glUniform4f(g_pGlobalState->trailShader.gradient, thisboxopengl.x, thisboxopengl.y, thisboxopengl.w, thisboxopengl.h);
    glUniform4f(g_pGlobalState->trailShader.color, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    CBox transformedBox = monbox;
    transformedBox.transform(wlTransformToHyprutils(invertTransform(g_pHyprOpenGL->m_renderData.pMonitor->m_transform)), g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.x,
                             g_pHyprOpenGL->m_renderData.pMonitor->m_transformedSize.y);

    glVertexAttribPointer(g_pGlobalState->trailShader.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, (float*)points.data());

    glEnableVertexAttribArray(g_pGlobalState->trailShader.posAttrib);

    if (g_pHyprOpenGL->m_renderData.clipBox.width != 0 && g_pHyprOpenGL->m_renderData.clipBox.height != 0) {
        CRegion damageClip{g_pHyprOpenGL->m_renderData.clipBox.x, g_pHyprOpenGL->m_renderData.clipBox.y, g_pHyprOpenGL->m_renderData.clipBox.width,
                           g_pHyprOpenGL->m_renderData.clipBox.height};
        damageClip.intersect(g_pHyprOpenGL->m_renderData.damage);

        if (!damageClip.empty()) {
            for (auto& RECT : damageClip.getRects()) {
                g_pHyprOpenGL->scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, points.size());
            }
        }
    } else {
        for (auto& RECT : g_pHyprOpenGL->m_renderData.damage.getRects()) {
            g_pHyprOpenGL->scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, points.size());
        }
    }

    glDisableVertexAttribArray(g_pGlobalState->trailShader.posAttrib);

    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDisable(GL_STENCIL_TEST);

    glStencilMask(-1);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    g_pHyprOpenGL->scissor(nullptr);

    // calculate damage
    float minX = 9999999;
    float minY = 9999999;
    float maxX = -9999999;
    float maxY = -9999999;

    for (auto& p : points) {
        if (p.x < minX)
            minX = p.x;
        if (p.y < minY)
            minY = p.y;
        if (p.x > maxX)
            maxX = p.x;
        if (p.y > maxY)
            maxY = p.y;
    }

    // bring back to global coords
    minX *= pMonitor->m_size.x;
    minY *= pMonitor->m_size.y;
    maxX *= pMonitor->m_size.x;
    maxY *= pMonitor->m_size.y;

    m_bLastBox.x      = minX + pMonitor->m_position.x;
    m_bLastBox.y      = minY + pMonitor->m_position.y;
    m_bLastBox.width  = maxX - minX;
    m_bLastBox.height = maxY - minY;

    m_bNeedsDamage = true;
}

eDecorationType CTrail::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CTrail::updateWindow(PHLWINDOW pWindow) {
    const auto PWORKSPACE = pWindow->m_workspace;

    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D();

    m_lastWindowPos  = pWindow->m_realPosition->value() + WORKSPACEOFFSET;
    m_lastWindowSize = pWindow->m_realSize->value();

    damageEntire();
}

void CTrail::damageEntire() {
    CBox dm = {(int)(m_lastWindowPos.x - m_seExtents.topLeft.x), (int)(m_lastWindowPos.y - m_seExtents.topLeft.y),
               (int)(m_lastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x), (int)m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(dm);
}
