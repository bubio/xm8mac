#include "ra_overlay.h"

#include <cassert>

int main()
{
	Xm8Ra::RaOverlay overlay;

	assert(!overlay.HasVisibleNotice(0));
	assert(overlay.VisibleNotice(0).empty());

	overlay.AddNotice("RA: test", 100, 50);
	assert(overlay.HasVisibleNotice(100));
	assert(overlay.VisibleNotice(120) == "RA: test");
	assert(!overlay.HasVisibleNotice(150));
	assert(overlay.VisibleNotice(150).empty());

	overlay.AddNotice("", 200, 50);
	assert(!overlay.HasVisibleNotice(200));

	Xm8Ra::RaOverlaySnapshot snapshot;
	snapshot.mode_enabled = true;
	snapshot.hardcore_enabled = true;
	snapshot.status_text = "RA: logged in";
	snapshot.user_name = "tester";
	snapshot.game_title = "Sample";
	snapshot.rich_presence = "Playing";
	overlay.SetSnapshot(snapshot);

	assert(overlay.Snapshot().mode_enabled);
	assert(overlay.Snapshot().hardcore_enabled);
	assert(overlay.Snapshot().status_text == "RA: logged in");
	assert(overlay.Snapshot().user_name == "tester");
	assert(overlay.Snapshot().game_title == "Sample");
	assert(overlay.Snapshot().rich_presence == "Playing");

	overlay.Clear();
	assert(!overlay.HasVisibleNotice(201));
	assert(!overlay.Snapshot().mode_enabled);
	assert(overlay.Snapshot().status_text.empty());
	return 0;
}
