#include "trail.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/Window.hpp>

#include "globals.hpp"

void CTrail::onTick() {
    static auto* const PHISTORYSTEP   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:history_step")->getDataStaticPtr();
    static auto* const PHISTORYPOINTS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:history_points")->getDataStaticPtr();

    m_iTimer++;

    if (m_iTimer > **PHISTORYSTEP) {
        m_dLastGeoms.push_front({box{(float)m_pWindow->m_vRealPosition.value().x, (float)m_pWindow->m_vRealPosition.value().y, (float)m_pWindow->m_vRealSize.value().x,
                                     (float)m_pWindow->m_vRealSize.value().y},
                                 std::chrono::system_clock::now()});
        while (m_dLastGeoms.size() > **PHISTORYPOINTS)
            m_dLastGeoms.pop_back();

        m_iTimer = 0;
    }

    if (m_bNeedsDamage) {
        g_pHyprRenderer->damageBox(&m_bLastBox);
        m_bNeedsDamage = false;
    }
}

CTrail::CTrail(CWindow* pWindow) : IHyprWindowDecoration(pWindow), m_pWindow(pWindow) {
    m_vLastWindowPos  = pWindow->m_vRealPosition.value();
    m_vLastWindowSize = pWindow->m_vRealSize.value();

    pTickCb = g_pHookSystem->hookDynamic("trailTick", [this](void* self, SCallbackInfo& info, std::any data) { this->onTick(); });
}

CTrail::~CTrail() {
    damageEntire();
    g_pHookSystem->unhook(pTickCb);
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

void CTrail::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    if (!g_pCompositor->windowValidMapped(m_pWindow))
        return;

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    static auto* const PBEZIERSTEP    = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:bezier_step")->getDataStaticPtr();
    static auto* const PPOINTSPERSTEP = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:points_per_step")->getDataStaticPtr();
    static auto* const PCOLOR         = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprtrails:color")->getDataStaticPtr();

    const CColor       COLOR = **PCOLOR;

    if (m_dLastGeoms.size() < 2)
        return;

    box  thisbox = box{(float)m_pWindow->m_vRealPosition.value().x, (float)m_pWindow->m_vRealPosition.value().y, (float)m_pWindow->m_vRealSize.value().x,
                      (float)m_pWindow->m_vRealSize.value().y};
    CBox wlrbox  = {thisbox.x - pMonitor->vecPosition.x, thisbox.y - pMonitor->vecPosition.y, thisbox.w, thisbox.h};
    wlrbox.scale(pMonitor->scale).round();

    g_pHyprOpenGL->scissor((CBox*)nullptr); // allow the entire window and stencil to render
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    g_pHyprOpenGL->renderRect(&wlrbox, CColor(0, 0, 0, 0), m_pWindow->rounding() * pMonitor->scale);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_NOTEQUAL, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    CBox  monbox = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.y};

    float matrix[9];
    wlr_matrix_project_box(matrix, monbox.pWlr(), wlr_output_transform_invert(WL_OUTPUT_TRANSFORM_NORMAL), 0,
                           g_pHyprOpenGL->m_RenderData.pMonitor->projMatrix.data()); // TODO: write own, don't use WLR here

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, g_pHyprOpenGL->m_RenderData.projection, matrix);

    g_pHyprOpenGL->blend(true);

    glUseProgram(g_pGlobalState->trailShader.program);

#ifndef GLES2
    glUniformMatrix3fv(g_pGlobalState->trailShader.proj, 1, GL_TRUE, glMatrix);
#else
    wlr_matrix_transpose(glMatrix, glMatrix);
    glUniformMatrix3fv(g_pGlobalState->trailShader.proj, 1, GL_FALSE, glMatrix);
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
        Vector2D diff = Vector2D{a.x - b.x, a.y - b.y} * pMonitor->vecSize;
        return std::sqrt(diff.x * diff.x + diff.y * diff.y);
    };

    float    msMaxToLast = msFrom(m_dLastGeoms.back().second);

    float    dists[2] = {0, 0};

    Vector2D mainVec      = {originalCoeff / pMonitor->vecSize.x, originalCoeff / pMonitor->vecSize.y};
    Vector2D windowMiddle = m_pWindow->middle() - pMonitor->vecPosition;

    points.push_back(
        Vector2D{cos(0) * mainVec.x - sin(0) * mainVec.y + windowMiddle.x / pMonitor->vecSize.x, sin(0) * mainVec.x + cos(0) * mainVec.y + windowMiddle.y / pMonitor->vecSize.y});
    points.push_back(Vector2D{cos(-M_PI_2) * mainVec.x - sin(-M_PI_2) * mainVec.y + windowMiddle.x / pMonitor->vecSize.x,
                              sin(-M_PI_2) * mainVec.x + cos(-M_PI_2) * mainVec.y + windowMiddle.y / pMonitor->vecSize.y});
    points.push_back(Vector2D{cos(M_PI_2) * mainVec.x - sin(M_PI_2) * mainVec.y + windowMiddle.x / pMonitor->vecSize.x,
                              sin(M_PI_2) * mainVec.x + cos(M_PI_2) * mainVec.y + windowMiddle.y / pMonitor->vecSize.y});
    points.push_back(Vector2D{cos(M_PI) * mainVec.x - sin(M_PI) * mainVec.y + windowMiddle.x / pMonitor->vecSize.x,
                              sin(M_PI) * mainVec.x + cos(M_PI) * mainVec.y + windowMiddle.y / pMonitor->vecSize.y});

    pointsForBezier.push_back(windowMiddle);
    agesForBezier.push_back(0);

    for (size_t i = 0; i < m_dLastGeoms.size(); i += 1) {
        box box = m_dLastGeoms[i].first;
        box.x -= pMonitor->vecPosition.x;
        box.y -= pMonitor->vecPosition.y;
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
        g_pHyprOpenGL->scissor((CBox*)nullptr);
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
            Vector2D newVec = {vecNormal.x * coeff / pMonitor->vecSize.x, vecNormal.y * coeff / pMonitor->vecSize.y};

            if ((newVec.x == 0 && newVec.y == 0) || std::isnan(newVec.x) || std::isnan(newVec.y))
                continue;

            // rotate by 90 and -90 and add middle
            points.push_back(Vector2D{cos(M_PI_2) * newVec.x - sin(M_PI_2) * newVec.y + middle.x / pMonitor->vecSize.x,
                                      sin(M_PI_2) * newVec.x + cos(M_PI_2) * newVec.y + middle.y / pMonitor->vecSize.y});
            points.push_back(Vector2D{cos(-M_PI_2) * newVec.x - sin(-M_PI_2) * newVec.y + middle.x / pMonitor->vecSize.x,
                                      sin(-M_PI_2) * newVec.x + cos(-M_PI_2) * newVec.y + middle.y / pMonitor->vecSize.y});
        }
    }

    box thisboxopengl = box{(m_pWindow->m_vRealPosition.value().x - pMonitor->vecPosition.x) / pMonitor->vecSize.x,
                            (m_pWindow->m_vRealPosition.value().y - pMonitor->vecPosition.y) / pMonitor->vecSize.y,
                            (m_pWindow->m_vRealPosition.value().x + m_pWindow->m_vRealSize.value().x) / pMonitor->vecSize.x,
                            (m_pWindow->m_vRealPosition.value().y + m_pWindow->m_vRealSize.value().y) / pMonitor->vecSize.y};
    glUniform4f(g_pGlobalState->trailShader.gradient, thisboxopengl.x, thisboxopengl.y, thisboxopengl.w, thisboxopengl.h);
    glUniform4f(g_pGlobalState->trailShader.color, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    CBox transformedBox = monbox;
    transformedBox.transform(wlr_output_transform_invert(g_pHyprOpenGL->m_RenderData.pMonitor->transform), g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.x,
                             g_pHyprOpenGL->m_RenderData.pMonitor->vecTransformedSize.y);

    glVertexAttribPointer(g_pGlobalState->trailShader.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, (float*)points.data());

    glEnableVertexAttribArray(g_pGlobalState->trailShader.posAttrib);

    if (g_pHyprOpenGL->m_RenderData.clipBox.width != 0 && g_pHyprOpenGL->m_RenderData.clipBox.height != 0) {
        CRegion damageClip{g_pHyprOpenGL->m_RenderData.clipBox.x, g_pHyprOpenGL->m_RenderData.clipBox.y, g_pHyprOpenGL->m_RenderData.clipBox.width,
                           g_pHyprOpenGL->m_RenderData.clipBox.height};
        damageClip.intersect(g_pHyprOpenGL->m_RenderData.damage);

        if (!damageClip.empty()) {
            for (auto& RECT : damageClip.getRects()) {
                g_pHyprOpenGL->scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, points.size());
            }
        }
    } else {
        for (auto& RECT : g_pHyprOpenGL->m_RenderData.damage.getRects()) {
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
    g_pHyprOpenGL->scissor((CBox*)nullptr);

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
    minX *= pMonitor->vecSize.x;
    minY *= pMonitor->vecSize.y;
    maxX *= pMonitor->vecSize.x;
    maxY *= pMonitor->vecSize.y;

    m_bLastBox.x      = minX + pMonitor->vecPosition.x;
    m_bLastBox.y      = minY + pMonitor->vecPosition.y;
    m_bLastBox.width  = maxX - minX;
    m_bLastBox.height = maxY - minY;

    m_bNeedsDamage = true;
}

eDecorationType CTrail::getDecorationType() {
    return DECORATION_CUSTOM;
}

void CTrail::updateWindow(CWindow* pWindow) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.value() : Vector2D();

    m_vLastWindowPos  = pWindow->m_vRealPosition.value() + WORKSPACEOFFSET;
    m_vLastWindowSize = pWindow->m_vRealSize.value();

    damageEntire();
}

void CTrail::damageEntire() {
    CBox dm = {(int)(m_vLastWindowPos.x - m_seExtents.topLeft.x), (int)(m_vLastWindowPos.y - m_seExtents.topLeft.y),
               (int)(m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x), (int)m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}