#include "ra_disk_transaction.h"

#include <cstdlib>
#include <iostream>

namespace {

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

} // namespace

int main()
{
	using namespace Xm8Ra;

	RaDiskTransactionState transaction;
	Check(!transaction.Active(), "new transaction is idle");
	Check(!transaction.OwnsReset(), "idle transaction owns no reset");

	transaction.Begin(RaDiskTransactionKind::Auxiliary,
		RaDiskTransactionPhase::VerifyingAuxiliary, true, true,
		RaDiskTransactionOperation::LibraryLaunch);
	Check(transaction.Pending(), "auxiliary verification is pending");
	Check(transaction.operation == RaDiskTransactionOperation::LibraryLaunch,
		"transaction owns the launch operation type");
	Check(transaction.OwnsReset(), "pending launch owns its reset");
	Check(!transaction.CanCompleteLaunch(),
		"verification alone cannot complete launch");
	transaction.Commit();
	Check(transaction.CanCompleteLaunch(),
		"only a committed transaction can complete launch");
	Check(!transaction.OwnsReset(), "commit consumes reset request");

	transaction.Clear();
	Check(!transaction.Active(), "clear discards committed transaction");

	transaction.Begin(RaDiskTransactionKind::Auxiliary,
		RaDiskTransactionPhase::VerifyingAuxiliary, true, true);
	transaction.Fail();
	Check(!transaction.CanCompleteLaunch(),
		"rejected auxiliary media cannot complete launch");
	Check(!transaction.OwnsReset(), "failure discards reset request");
	transaction.Clear();
	Check(!transaction.Active(), "failure leaves no state for the next drop");

	transaction.Begin(RaDiskTransactionKind::Anchor,
		RaDiskTransactionPhase::ChangingActiveMedia, true, false);
	transaction.phase = RaDiskTransactionPhase::RollingBack;
	Check(transaction.Pending(), "rollback remains part of the transaction");
	transaction.Fail();
	Check(!transaction.OwnsReset(), "rollback failure discards reset request");

	auto CheckTerminalFailure = [](const char *label) {
		RaDiskTransactionState failed;
		failed.Begin(RaDiskTransactionKind::Auxiliary,
			RaDiskTransactionPhase::VerifyingAuxiliary, true, true);
		failed.Fail();
		Check(!failed.CanCompleteLaunch(), label);
		Check(!failed.OwnsReset(), label);
		failed.Clear();
		Check(!failed.Active(), label);
	};
	CheckTerminalFailure("different-game rejection is terminal");
	CheckTerminalFailure("unidentified-media rejection is terminal");
	CheckTerminalFailure("verification unavailability is terminal");
	CheckTerminalFailure("VM commit failure is terminal");
	CheckTerminalFailure("RA rollback failure is terminal");
	CheckTerminalFailure("cancel is terminal");

	RaDiskTransactionState rollback_succeeded;
	rollback_succeeded.Begin(RaDiskTransactionKind::Anchor,
		RaDiskTransactionPhase::ChangingActiveMedia, true, false);
	rollback_succeeded.phase = RaDiskTransactionPhase::RollingBack;
	rollback_succeeded.Clear();
	Check(!rollback_succeeded.Active(),
		"successful rollback clears transaction and reset");

	Check(!ShouldDeferLibraryDrive2ForRa(false, true, true, true, true, true),
		"RA OFF mounts a paired Library profile immediately even when Hardcore is selected");
	Check(!ShouldForceLibraryOfflineForRa(false, true, true, false),
		"RA OFF never enters an RA Offline session for a Library launch");
	Check(ShouldDeferLibraryDrive2ForRa(true, true, true, true, true, true),
		"reachable Hardcore RA launch defers Drive 2 verification");
	Check(ShouldForceLibraryOfflineForRa(true, true, true, false),
		"unreachable logged-in RA launch enters Offline before mounting");
	Check(!ShouldDeferLibraryDrive2ForRa(true, true, false, true, true, true),
		"Casual RA launch mounts a paired Library profile immediately");
	Check(!CanBeginAuxiliaryVerification(true, false),
		"Library Drive 2 verification waits for the anchor game load");
	Check(CanBeginAuxiliaryVerification(true, true),
		"Library Drive 2 verification begins after the anchor game load");
	Check(CanBeginAuxiliaryVerification(false, false),
		"active-session Drive 2 verification does not wait for a new game load");

	return 0;
}
