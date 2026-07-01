//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// [ RetroAchievements overlay state ]
//

#include "ra_overlay.h"

namespace Xm8Ra {

void RaOverlay::Clear()
{
	notice_text_.clear();
	notice_until_ms_ = 0;
	snapshot_ = RaOverlaySnapshot();
}

void RaOverlay::AddNotice(const std::string& text, uint32_t now_ms,
	uint32_t duration_ms)
{
	if (text.empty()) {
		return;
	}
	notice_text_ = text;
	notice_until_ms_ = now_ms + duration_ms;
}

bool RaOverlay::HasVisibleNotice(uint32_t now_ms) const
{
	return !notice_text_.empty() &&
		static_cast<int32_t>(now_ms - notice_until_ms_) < 0;
}

std::string RaOverlay::VisibleNotice(uint32_t now_ms) const
{
	return HasVisibleNotice(now_ms) ? notice_text_ : std::string();
}

void RaOverlay::SetSnapshot(const RaOverlaySnapshot& snapshot)
{
	snapshot_ = snapshot;
}

const RaOverlaySnapshot& RaOverlay::Snapshot() const
{
	return snapshot_;
}

} // namespace Xm8Ra
