#ifndef XM8_RA_HTTP_ANDROID_H
#define XM8_RA_HTTP_ANDROID_H

#include "ra_http_client.h"

#include <memory>
#include <string>

namespace Xm8Ra {

std::unique_ptr<RaHttpClient> CreateAndroidRaHttpClient(
	const std::string& user_agent);

} // namespace Xm8Ra

#endif
