#include "ra_platform.h"

#ifdef __APPLE__
#include "ra_http_mac.h"
#endif

namespace Xm8Ra {

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

} // namespace Xm8Ra
