#ifndef XM8_RA_PLATFORM_H
#define XM8_RA_PLATFORM_H

#include "ra_credentials.h"
#include "ra_http_client.h"

#include <memory>
#include <string>

namespace Xm8Ra {

std::unique_ptr<RaHttpClient> CreatePlatformRaHttpClient(
	const std::string& user_agent);

} // namespace Xm8Ra

#endif
