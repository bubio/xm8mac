#include "ra_platform.h"

#include "ra_connectivity.h"

#ifdef __APPLE__
#include "ra_http_mac.h"
#elif defined(_WIN32)
#include "ra_http_win.h"
#endif

namespace Xm8Ra {

#ifdef __APPLE__
std::unique_ptr<RaConnectivityMonitor> CreateMacRaConnectivityMonitor();
#elif defined(_WIN32)
std::unique_ptr<RaConnectivityMonitor> CreateWinRaConnectivityMonitor();
#endif

std::unique_ptr<RaHttpClient> CreatePlatformRaHttpClient(
	const std::string& user_agent)
{
#ifdef __APPLE__
	return CreateMacRaHttpClient(user_agent);
#elif defined(_WIN32)
	return CreateWinRaHttpClient(user_agent);
#else
	(void)user_agent;
	return nullptr;
#endif
}

std::unique_ptr<RaConnectivityMonitor> CreatePlatformRaConnectivityMonitor()
{
#ifdef __APPLE__
	return CreateMacRaConnectivityMonitor();
#elif defined(_WIN32)
	return CreateWinRaConnectivityMonitor();
#else
	return nullptr;
#endif
}

} // namespace Xm8Ra
