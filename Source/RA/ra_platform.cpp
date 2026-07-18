#include "ra_platform.h"

#include "ra_connectivity.h"

#ifdef __APPLE__
#include "ra_http_mac.h"
#endif

namespace Xm8Ra {

#ifdef __APPLE__
std::unique_ptr<RaConnectivityMonitor> CreateMacRaConnectivityMonitor();
#endif

std::unique_ptr<RaHttpClient> CreatePlatformRaHttpClient(
	const std::string& user_agent)
{
#ifdef __APPLE__
	return CreateMacRaHttpClient(user_agent);
#else
	(void)user_agent;
	return nullptr;
#endif
}

std::unique_ptr<RaConnectivityMonitor> CreatePlatformRaConnectivityMonitor()
{
#ifdef __APPLE__
	return CreateMacRaConnectivityMonitor();
#else
	return nullptr;
#endif
}

} // namespace Xm8Ra
