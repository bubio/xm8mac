#ifndef XM8_RA_MENU_STATUS_H
#define XM8_RA_MENU_STATUS_H

#include <string>

namespace Xm8Ra {

enum class RaMenuStatusState {
	Unavailable,
	Disabled,
	Enabled,
	LoginPending,
	PendingGame,
	ActiveGame,
	UnknownGame,
	OfflineSession,
	Disconnected,
	LoggedIn,
	LoginFailed,
	SubmissionError,
};

class RaMenuStatus {
public:
	RaMenuStatus() : state_(RaMenuStatusState::Unavailable) {}

	void Set(RaMenuStatusState state, const std::string& detail = std::string())
	{
		state_ = state;
		detail_ = detail;
	}

	void EnterOfflineSession()
	{
		if (state_ != RaMenuStatusState::UnknownGame) {
			Set(RaMenuStatusState::OfflineSession);
		}
	}

	void SetConnectivity(bool disconnected)
	{
		Set(disconnected ? RaMenuStatusState::Disconnected :
			RaMenuStatusState::ActiveGame, detail_);
	}

	RaMenuStatusState State() const
	{
		return state_;
	}

	const std::string& Detail() const
	{
		return detail_;
	}

private:
	RaMenuStatusState state_;
	std::string detail_;
};

} // namespace Xm8Ra

#endif
