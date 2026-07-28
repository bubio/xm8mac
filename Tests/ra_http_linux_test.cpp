#include "ra_connectivity.h"
#include "ra_http_linux.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Xm8Ra {
std::unique_ptr<RaConnectivityMonitor> CreateLinuxRaConnectivityMonitor();
}

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

class LocalTcpServer {
public:
	explicit LocalTcpServer(bool send_plaintext) :
		send_plaintext_(send_plaintext)
	{
		listen_ = socket(AF_INET, SOCK_STREAM, 0);
		if (listen_ < 0) return;
		const int reuse = 1;
		setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = 0;
		if (bind(listen_, reinterpret_cast<sockaddr *>(&address),
			sizeof(address)) != 0 || listen(listen_, 1) != 0) {
			close(listen_);
			listen_ = -1;
			return;
		}
		socklen_t size = sizeof(address);
		if (getsockname(listen_, reinterpret_cast<sockaddr *>(&address),
			&size) != 0) return;
		port_ = ntohs(address.sin_port);
		fcntl(listen_, F_SETFL, fcntl(listen_, F_GETFL, 0) | O_NONBLOCK);
		thread_ = std::thread(&LocalTcpServer::Run, this);
	}

	~LocalTcpServer()
	{
		stop_ = true;
		const int client = client_.exchange(-1);
		if (client >= 0) {
			shutdown(client, SHUT_RDWR);
			close(client);
		}
		if (thread_.joinable()) thread_.join();
		if (listen_ >= 0) close(listen_);
	}

	bool Valid() const { return port_ != 0; }
	std::string Url() const
	{
		return "https://127.0.0.1:" + std::to_string(port_) + "/test";
	}

private:
	void Run()
	{
		int client = -1;
		while (!stop_ && client < 0) {
			client = accept(listen_, nullptr, nullptr);
			if (client < 0) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		if (client < 0) return;
		client_ = client;
		if (send_plaintext_) {
			const char response[] =
				"HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
			send(client, response, sizeof(response) - 1, MSG_NOSIGNAL);
			return;
		}
		char buffer[256];
		while (!stop_ && recv(client, buffer, sizeof(buffer), MSG_DONTWAIT) >= 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}

	bool send_plaintext_ = false;
	std::atomic<bool> stop_{false};
	int listen_ = -1;
	std::atomic<int> client_{-1};
	unsigned short port_ = 0;
	std::thread thread_;
};

bool WaitForResponse(Xm8Ra::RaHttpClient *client,
	Xm8Ra::RaHttpResponse *response, int timeout_ms)
{
	const auto deadline = std::chrono::steady_clock::now() +
		std::chrono::milliseconds(timeout_ms);
	while (std::chrono::steady_clock::now() < deadline) {
		std::vector<Xm8Ra::RaHttpResponse> completed;
		client->DrainCompleted(&completed);
		if (!completed.empty()) {
			*response = std::move(completed.front());
			return true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return false;
}

} // namespace

int main()
{
	{
		auto client = Xm8Ra::CreateLinuxRaHttpClient("XM8/phase10-test");
		Check(client != nullptr, "create Linux HTTP client");
		Xm8Ra::RaHttpRequest request;
		request.request_id = 1;
		request.url = "http://127.0.0.1/test";
		client->Send(request);
		Xm8Ra::RaHttpResponse response;
		Check(WaitForResponse(client.get(), &response, 1000),
			"non-HTTPS request completes");
		Check(response.transport_result == Xm8Ra::RaHttpTransportResult::ClientError,
			"non-HTTPS request is rejected");
	}

	{
		LocalTcpServer server(true);
		Check(server.Valid(), "start invalid TLS server");
		auto client = Xm8Ra::CreateLinuxRaHttpClient("XM8/phase10-test");
		Xm8Ra::RaHttpRequest request;
		request.request_id = 2;
		request.url = server.Url();
		request.connect_timeout_ms = 500;
		request.total_timeout_ms = 500;
		client->Send(request);
		Xm8Ra::RaHttpResponse response;
		Check(WaitForResponse(client.get(), &response, 3000),
			"invalid TLS request completes");
		Check(response.transport_result != Xm8Ra::RaHttpTransportResult::Success &&
			response.http_status == 0, "invalid TLS is rejected");
	}

	{
		LocalTcpServer server(false);
		auto client = Xm8Ra::CreateLinuxRaHttpClient("XM8/phase10-test");
		Xm8Ra::RaHttpRequest request;
		request.request_id = 3;
		request.url = server.Url();
		client->Send(request);
		client->Cancel(request.request_id);
		Xm8Ra::RaHttpResponse response;
		Check(WaitForResponse(client.get(), &response, 3000),
			"canceled request completes");
		Check(response.transport_result == Xm8Ra::RaHttpTransportResult::Canceled,
			"request cancellation classified");
	}

	{
		LocalTcpServer server(false);
		Check(server.Valid(), "start timeout server");
		auto client = Xm8Ra::CreateLinuxRaHttpClient("XM8/phase10-test");
		Xm8Ra::RaHttpRequest request;
		request.request_id = 4;
		request.url = server.Url();
		request.connect_timeout_ms = 200;
		request.total_timeout_ms = 200;
		client->Send(request);
		Xm8Ra::RaHttpResponse response;
		Check(WaitForResponse(client.get(), &response, 3000),
			"stalled TLS request completes");
		Check(response.transport_result == Xm8Ra::RaHttpTransportResult::Timeout,
			"stalled TLS request is classified as timeout");
	}

	{
		LocalTcpServer server(false);
		const auto start = std::chrono::steady_clock::now();
		{
			auto client = Xm8Ra::CreateLinuxRaHttpClient("XM8/phase10-test");
			Xm8Ra::RaHttpRequest request;
			request.request_id = 5;
			request.url = server.Url();
			request.total_timeout_ms = 30000;
			client->Send(request);
		}
		Check(std::chrono::steady_clock::now() - start < std::chrono::seconds(2),
			"client shutdown cancels active request promptly");
	}

	const char *live_url = std::getenv("XM8_RA_HTTP_LIVE_TEST_URL");
	if (live_url != nullptr && live_url[0] != '\0') {
		auto client = Xm8Ra::CreateLinuxRaHttpClient("XM8/phase10-live-test");
		Xm8Ra::RaHttpRequest request;
		request.request_id = 6;
		request.url = live_url;
		request.total_timeout_ms = 10000;
		client->Send(request);
		Xm8Ra::RaHttpResponse response;
		Check(WaitForResponse(client.get(), &response, 15000),
			"live HTTPS request completes");
		if (response.transport_result != Xm8Ra::RaHttpTransportResult::Success ||
			response.http_status == 0) {
			std::cerr << "live HTTPS result=" <<
				static_cast<int>(response.transport_result) <<
				" status=" << response.http_status <<
				" error=" << response.error << '\n';
		}
		Check(response.transport_result == Xm8Ra::RaHttpTransportResult::Success &&
			response.http_status > 0,
			"live HTTPS and system CA succeed");
	}

	{
		auto monitor = Xm8Ra::CreateLinuxRaConnectivityMonitor();
		Check(monitor != nullptr, "create Linux connectivity monitor");
		Check(monitor->Poll() != Xm8Ra::RaReachabilityState::Unknown,
			"Linux connectivity poll completes");
	}

	return failures == 0 ? 0 : 1;
}
