#include "ra_credentials.h"
#include "ra_http_fake.h"
#include "ra_service.h"

#include "rc_error.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

bool MakeDirectory(const std::string& path)
{
	return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

std::string TemporaryRoot(const char *name)
{
	const auto unique = std::chrono::steady_clock::now()
		.time_since_epoch().count();
	const char *temporary = std::getenv(
#ifdef _WIN32
		"TEMP"
#else
		"TMPDIR"
#endif
	);
	return std::string(temporary != nullptr ? temporary :
#ifdef _WIN32
		"."
#else
		"/tmp"
#endif
	) + "/" + name + "-" + std::to_string(unique);
}

std::unique_ptr<Xm8Ra::FakeRaHttpClient> MakeFakeHttp()
{
	return std::unique_ptr<Xm8Ra::FakeRaHttpClient>(
		new Xm8Ra::FakeRaHttpClient());
}

Xm8Ra::RaHttpResponse MakeJsonResponse(uint64_t request_id,
	const std::string& json)
{
	Xm8Ra::RaHttpResponse response;
	response.request_id = request_id;
	response.http_status = 200;
	response.transport_result = Xm8Ra::RaHttpTransportResult::Success;
	response.body.assign(json.begin(), json.end());
	return response;
}

} // namespace

int main()
{
	const std::string base = TemporaryRoot("xm8-ra-service");
	Check(MakeDirectory(base), "create service test root");

	Xm8Ra::RaCredentialsStore credential_store(base);

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		std::string error;
		Check(service.IsReady(), "service is ready");
		Check(service.BeginLoginWithPassword("player", "secret-password",
			&error), "begin password login");
		Check(fake_http_raw->SentRequests().size() == 1,
			"password login sends HTTP request");
		Check(fake_http_raw->SentRequests()[0].post_data.find(
			"p=secret-password") != std::string::npos,
			"password login request contains password only in HTTP body");
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoginPending,
			"password login pending");

		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"saved-token\","
			"\"Score\":10,\"SoftcoreScore\":20,\"Messages\":0}"));
		service.DrainHttp();
		const Xm8Ra::RaLoginSnapshot snapshot = service.LoginSnapshot();
		Check(snapshot.state == Xm8Ra::RaLoginState::LoggedIn,
			"password login succeeds");
		Check(snapshot.username == "player", "logged in username captured");

		Xm8Ra::RaCredentials loaded;
		Check(credential_store.Load(&loaded, &error),
			"password login saves credentials");
		Check(loaded.username == "player" && loaded.token == "saved-token",
			"saved credentials use returned token");

		service.Shutdown();
		Check(service.PendingHttpCount() == 0, "shutdown clears pending HTTP");
	}

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		std::string error;
		Check(service.BeginLoginWithSavedToken(&error),
			"begin saved token login");
		Check(fake_http_raw->SentRequests().size() == 1,
			"saved token login sends HTTP request");
		Check(fake_http_raw->SentRequests()[0].post_data.find(
			"t=saved-token") != std::string::npos,
			"saved token login request uses token");
		Check(fake_http_raw->SentRequests()[0].post_data.find(
			"p=") == std::string::npos,
			"saved token login does not send password field");

		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":false,\"Error\":\"Invalid token\","
			"\"Code\":\"invalid_credentials\"}"));
		service.DrainHttp();
		const Xm8Ra::RaLoginSnapshot snapshot = service.LoginSnapshot();
		Check(snapshot.state == Xm8Ra::RaLoginState::Failed,
			"saved token rejection fails login");
		Check(snapshot.credentials_deleted,
			"saved token rejection deletes credentials");

		Xm8Ra::RaCredentials loaded;
		Check(!credential_store.Load(&loaded, &error),
			"credentials file removed after rejected token");
	}

	{
		Xm8Ra::RaCredentials credentials;
		credentials.username = "player";
		credentials.token = "logout-token";
		std::string error;
		Check(credential_store.Save(credentials, &error),
			"save credentials for logout");

		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		Check(service.BeginLoginWithSavedToken(&error),
			"begin saved token login for logout");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"logout-token\","
			"\"Score\":1,\"SoftcoreScore\":2,\"Messages\":0}"));
		service.DrainHttp();
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoggedIn,
			"saved token login succeeds before logout");

		service.Logout();
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoggedOut,
			"logout resets login state");
		Xm8Ra::RaCredentials loaded;
		Check(!credential_store.Load(&loaded, &error),
			"logout deletes saved token");
	}

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		std::string error;
		Check(service.BeginLoginWithPassword("player", "pending-secret",
			&error), "begin pending login");
		Check(service.PendingHttpCount() == 1, "pending login has HTTP call");
		service.Shutdown();
		Check(service.PendingHttpCount() == 0,
			"shutdown drains canceled request");
	}

	std::remove(credential_store.Path().c_str());
#ifndef _WIN32
	rmdir(base.c_str());
#endif

	if (failures != 0) {
		std::cerr << failures << " RA service test failure(s)\n";
		return 1;
	}

	std::cout << "RA service tests passed\n";
	return 0;
}
