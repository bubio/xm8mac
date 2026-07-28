#ifndef XM8_RA_HTTP_CLIENT_H
#define XM8_RA_HTTP_CLIENT_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace Xm8Ra {

enum class RaHttpPurpose {
	Api,
	Image
};

enum class RaHttpTransportResult {
	Success = 0,
	ClientError = 1,
	RetryableClientError = 2,
	Canceled = 3,
	Timeout = 4,
	Oversize = 5
};

struct RaHttpRequest {
	uint64_t request_id = 0;
	RaHttpPurpose purpose = RaHttpPurpose::Api;
	std::string url;
	bool has_post_data = false;
	std::string post_data;
	std::string content_type;
	uint32_t connect_timeout_ms = 10000;
	uint32_t total_timeout_ms = 30000;
	size_t max_response_bytes = 8U * 1024U * 1024U;
};

struct RaHttpResponse {
	uint64_t request_id = 0;
	RaHttpTransportResult transport_result = RaHttpTransportResult::Success;
	int http_status = 0;
	std::string content_type;
	std::vector<uint8_t> body;
	std::string error;
};

class RaHttpClient {
public:
	virtual ~RaHttpClient() = default;

	virtual void Send(const RaHttpRequest& request) = 0;
	virtual void Cancel(uint64_t request_id) = 0;
	virtual void CancelAll() = 0;
	virtual void DrainCompleted(std::vector<RaHttpResponse> *output) = 0;
};

} // namespace Xm8Ra

#endif
