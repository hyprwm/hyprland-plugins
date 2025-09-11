#include "ExpoGesture.hpp"

#include "overview.hpp"

void CExpoGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_lastDelta = 0.F;
}

void CExpoGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    m_lastDelta += distance(e);

    if (m_lastDelta <= 0.01) // plugin will crash if swipe ends at <= 0
        m_lastDelta = 0.01;

    g_pOverview->onSwipeUpdate(m_lastDelta);
}

void CExpoGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    g_pOverview->onSwipeEnd();
}
