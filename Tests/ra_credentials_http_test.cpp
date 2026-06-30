#include "ra_credentials.h"
#include "ra_http_fake.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <string>
#include <vector>

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

bool WriteByte(const std::string& path, long offset, char value)
{
	std::fstream stream(path, std::ios::binary | std::ios::in | std::ios::out);
	if (!stream.is_open()) {
		return false;
	}
	stream.seekp(offset);
	stream.put(value);
	return stream.good();
}

std::string LongString(size_t size, char value)
{
	return std::string(size, value);
}

} // namespace

int main()
{
	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
	const char *temporary = std::getenv(
#ifdef _WIN32
		"TEMP"
#else
		"TMPDIR"
#endif
	);
	const std::string base = std::string(temporary != nullptr ? temporary :
#ifdef _WIN32
		"."
#else
		"/tmp"
#endif
	) + "/xm8-ra-credentials-http-" + std::to_string(unique);

	Check(MakeDirectory(base), "create test root");
	Xm8Ra::RaCredentialsStore store(base);

	std::string error;
	Xm8Ra::RaCredentials saved;
	saved.username = "player";
	saved.token = "token-secret";
	Check(store.Save(saved, &error), "save credentials");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}

	Xm8Ra::RaCredentials loaded;
	Check(store.Load(&loaded, &error), "load credentials");
	Check(loaded.username == saved.username, "loaded username");
	Check(loaded.token == saved.token, "loaded token");
#ifndef _WIN32
	struct stat st;
	Check(stat(store.Path().c_str(), &st) == 0, "stat credentials");
	Check((st.st_mode & 0777) == 0600, "credentials created with 0600");
#endif

	Check(WriteByte(store.Path(), 8, '\xff'), "corrupt credentials length");
	Xm8Ra::RaCredentials invalid;
	Check(!store.Load(&invalid, &error), "reject invalid credentials");
	Check(invalid.username.empty() && invalid.token.empty(),
		"invalid credentials not returned");

	Check(!store.Save({ LongString(257, 'u'), "token" }, &error),
		"reject oversized username");
	Check(!store.Save({ "player", LongString(4097, 't') }, &error),
		"reject oversized token");

	Check(store.Save(saved, &error), "save credentials again");
	store.ClearSecret(&loaded);
	Check(loaded.token.empty(), "clear in-memory token");
	Check(store.Delete(&error), "delete credentials");
	Check(!store.Load(&loaded, &error), "deleted credentials not loaded");

	Xm8Ra::FakeRaHttpClient http;
	Xm8Ra::RaHttpRequest get;
	get.request_id = 1;
	get.url = "https://retroachievements.org/API/test";
	http.Send(get);

	Xm8Ra::RaHttpRequest empty_post;
	empty_post.request_id = 2;
	empty_post.url = get.url;
	empty_post.has_post_data = true;
	empty_post.content_type = "application/x-www-form-urlencoded";
	http.Send(empty_post);

	Xm8Ra::RaHttpRequest normal_post = empty_post;
	normal_post.request_id = 3;
	normal_post.post_data = "u=player&t=token-secret";
	http.Send(normal_post);
	normal_post.post_data = "mutated";

	Check(http.SentRequests().size() == 3, "fake captured requests");
	Check(!http.SentRequests()[0].has_post_data, "GET has no post data");
	Check(http.SentRequests()[1].has_post_data &&
		http.SentRequests()[1].post_data.empty(),
		"empty POST remains POST");
	Check(http.SentRequests()[2].post_data == "u=player&t=token-secret",
		"request body is owned copy");

	Xm8Ra::RaHttpResponse response;
	response.request_id = 1;
	response.http_status = 204;
	http.Complete(response);
	http.Cancel(2);
	response.request_id = 2;
	response.http_status = 200;
	http.Complete(response);

	std::vector<Xm8Ra::RaHttpResponse> completed;
	http.DrainCompleted(&completed);
	Check(completed.size() == 2, "drain completed responses");
	Check(completed[0].request_id == 1 &&
		completed[0].transport_result == Xm8Ra::RaHttpTransportResult::Success,
		"success response drained");
	Check(completed[1].request_id == 2 &&
		completed[1].transport_result == Xm8Ra::RaHttpTransportResult::Canceled,
		"cancel response drained once");

	std::remove(store.Path().c_str());
#ifndef _WIN32
	rmdir(base.c_str());
#endif
	return failures == 0 ? 0 : 1;
}
