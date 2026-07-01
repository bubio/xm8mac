//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// [ RetroAchievements overlay state ]
//

#ifndef XM8_RA_OVERLAY_H
#define XM8_RA_OVERLAY_H

#include <cstdint>
#include <string>

namespace Xm8Ra {

struct RaOverlaySnapshot {
	bool mode_enabled = false;
	bool hardcore_enabled = false;
	std::string status_text;
	std::string user_name;
	std::string game_title;
	std::string rich_presence;
};

class RaOverlay {
public:
	void Clear();
	void AddNotice(const std::string& text, uint32_t now_ms,
		uint32_t duration_ms = 5000);
	bool HasVisibleNotice(uint32_t now_ms) const;
	std::string VisibleNotice(uint32_t now_ms) const;

	void SetSnapshot(const RaOverlaySnapshot& snapshot);
	const RaOverlaySnapshot& Snapshot() const;

private:
	std::string notice_text_;
	uint32_t notice_until_ms_ = 0;
	RaOverlaySnapshot snapshot_;
};

} // namespace Xm8Ra

#endif // XM8_RA_OVERLAY_H
