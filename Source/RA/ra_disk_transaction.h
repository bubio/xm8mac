#ifndef XM8_RA_DISK_TRANSACTION_H
#define XM8_RA_DISK_TRANSACTION_H

namespace Xm8Ra {

enum class RaDiskTransactionKind {
	None,
	Anchor,
	Auxiliary,
};

enum class RaDiskTransactionOperation {
	None,
	AnchorMediaChange,
	AuxiliaryMount,
	PairedOpen,
	LibraryLaunch,
};

enum class RaDiskProfileUpdate {
	None,
	Auxiliary,
	Pair,
};

enum class RaDiskTransactionPhase {
	Idle,
	VerifyingAuxiliary,
	ChangingActiveMedia,
	CommittingVm,
	RollingBack,
	Committed,
	Failed,
};

// Pure transaction lifecycle shared by the UI coordinator and its tests.
// Reset ownership is deliberately part of the transaction so a terminal path
// cannot leave a reset request behind for a later drag-and-drop operation.
struct RaDiskTransactionState {
	RaDiskTransactionKind kind = RaDiskTransactionKind::None;
	RaDiskTransactionOperation operation = RaDiskTransactionOperation::None;
	RaDiskTransactionPhase phase = RaDiskTransactionPhase::Idle;
	bool reset_requested = false;
	bool launch_completion_required = false;

	bool Active() const
	{
		return kind != RaDiskTransactionKind::None;
	}

	bool IsAnchor() const
	{
		return kind == RaDiskTransactionKind::Anchor;
	}

	bool IsAuxiliary() const
	{
		return kind == RaDiskTransactionKind::Auxiliary;
	}

	bool Pending() const
	{
		return Active() && phase != RaDiskTransactionPhase::Committed &&
			phase != RaDiskTransactionPhase::Failed;
	}

	bool OwnsReset() const
	{
		return Pending() && reset_requested;
	}

	bool CanCompleteLaunch() const
	{
		return launch_completion_required &&
			phase == RaDiskTransactionPhase::Committed;
	}

	void Begin(RaDiskTransactionKind new_kind,
		RaDiskTransactionPhase new_phase, bool request_reset,
		bool completes_launch,
		RaDiskTransactionOperation new_operation =
			RaDiskTransactionOperation::None)
	{
		kind = new_kind;
		operation = new_operation;
		phase = new_phase;
		reset_requested = request_reset;
		launch_completion_required = completes_launch;
	}

	void Commit()
	{
		phase = RaDiskTransactionPhase::Committed;
		reset_requested = false;
	}

	void Fail()
	{
		phase = RaDiskTransactionPhase::Failed;
		reset_requested = false;
	}

	void Clear()
	{
		*this = RaDiskTransactionState();
	}
};

// Library launches must only defer a paired Drive 2 for an active RA session.
// Hardcore is a persisted preference, so it can remain selected while RA mode
// itself is OFF. In that case no RA service tick will advance a transaction.
inline bool ShouldDeferLibraryDrive2ForRa(bool ra_mode_enabled,
	bool drive2_assigned, bool hardcore_selected, bool service_available,
	bool logged_in, bool reachable)
{
	return ra_mode_enabled && drive2_assigned && hardcore_selected &&
		service_available && logged_in && reachable;
}

inline bool ShouldForceLibraryOfflineForRa(bool ra_mode_enabled,
	bool service_available, bool logged_in, bool reachable)
{
	return ra_mode_enabled && service_available && logged_in && !reachable;
}

// A library launch first loads its Drive 1 anchor through rc_client. That
// load advances the HTTP bridge generation, so its deferred Drive 2 hash
// request must not start until the anchor game is loaded.
inline bool CanBeginAuxiliaryVerification(bool completes_library_launch,
	bool anchor_game_loaded)
{
	return !completes_library_launch || anchor_game_loaded;
}

} // namespace Xm8Ra

#endif
