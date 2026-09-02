#ifndef XM8_RA_DISK_TRANSACTION_H
#define XM8_RA_DISK_TRANSACTION_H

#include <cstddef>

namespace Xm8Ra {

enum class RaDiskTransactionKind { None, Anchor, Auxiliary };
enum class RaDiskTransactionOperation {
	None, AnchorMediaChange, AuxiliaryMount, PairedOpen, LibraryLaunch,
};
enum class RaDiskProfileUpdate { None, Auxiliary, Pair };

enum class RaDiskTransactionPhase {
	Idle,
	VerifyingDrive2,
	VerifyingAuxiliary = VerifyingDrive2,
	ChangingActiveMedia,
	CommittingVm,
	RollingBack,
	Committed,
	Failed,
};

// One request owns every asynchronous RA operation and the eventual two-drive
// VM commit. No caller is permitted to start a second request while Active().
struct RaDiskTransactionState {
	RaDiskTransactionKind kind = RaDiskTransactionKind::None;
	RaDiskTransactionOperation operation = RaDiskTransactionOperation::None;
	RaDiskTransactionPhase phase = RaDiskTransactionPhase::Idle;
	bool reset_requested = false;
	bool launch_completion_required = false;

	bool Active() const { return phase != RaDiskTransactionPhase::Idle; }
	bool IsAnchor() const { return kind == RaDiskTransactionKind::Anchor; }
	bool IsAuxiliary() const { return kind == RaDiskTransactionKind::Auxiliary; }
	bool Pending() const { return Active(); }
	bool OwnsReset() const { return Active() && reset_requested; }
	bool CanCompleteLaunch() const
	{
		return launch_completion_required && phase == RaDiskTransactionPhase::Committed;
	}
	void Begin(RaDiskTransactionPhase next, bool reset, bool completes_launch)
	{
		phase = next;
		reset_requested = reset;
		launch_completion_required = completes_launch;
	}
	void Begin(RaDiskTransactionKind new_kind, RaDiskTransactionPhase next,
		bool reset, bool completes_launch,
		RaDiskTransactionOperation new_operation = RaDiskTransactionOperation::None)
	{
		kind = new_kind;
		operation = new_operation;
		Begin(next, reset, completes_launch);
	}
	void Clear() { *this = RaDiskTransactionState(); }
	void Commit() { phase = RaDiskTransactionPhase::Committed; reset_requested = false; }
	void Fail() { phase = RaDiskTransactionPhase::Failed; reset_requested = false; }
};

inline bool CanBeginAuxiliaryVerification(bool completes_library_launch,
	bool anchor_game_loaded)
{
	return !completes_library_launch || anchor_game_loaded;
}

inline bool ShouldDeferLibraryDrive2ForRa(bool ra_mode_enabled,
	bool drive2_assigned, bool /*hardcore_selected*/, bool service_available,
	bool logged_in, bool reachable)
{
	return ra_mode_enabled && drive2_assigned && service_available &&
		logged_in && reachable;
}

inline bool ShouldForceLibraryOfflineForRa(bool ra_mode_enabled,
	bool service_available, bool logged_in, bool reachable)
{
	return ra_mode_enabled && service_available && logged_in && !reachable;
}

inline bool CanApplySequentialRaMediaBatch(bool /*online_session*/,
	size_t /*target_count*/, bool /*closes_drive2*/)
{
	// A normalized two-drive request is transactional; reject no valid input
	// merely because it changes both drives.
	return true;
}

inline bool ShouldResetDroppedVmWithoutRestartingRaSession(
	bool session_offline, bool anchor_launch_pending)
{
	return session_offline || anchor_launch_pending;
}

struct RaSynchronousAnchorCommitPlan {
	bool reset_vm_now = false;
	bool begin_session = false;
};

inline RaSynchronousAnchorCommitPlan PlanSynchronousAnchorCommit(
	bool starts_new_anchor, bool caller_will_reset)
{
	return {starts_new_anchor && !caller_will_reset, starts_new_anchor};
}

} // namespace Xm8Ra

#endif
