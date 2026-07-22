#ifndef XM8_RA_HTTP_WIN_H
#define XM8_RA_HTTP_WIN_H

#include "ra_http_client.h"

#include <memory>
#include <string>

namespace Xm8Ra {

std::unique_ptr<RaHttpClient> CreateWinRaHttpClient(
	const std::string& user_agent);

} // namespace Xm8Ra

#endif
