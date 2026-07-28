#include "ra_credentials.h"
#include "ra_file_util.h"

#include <windows.h>
#include <wincred.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

std::wstring Utf8ToWide(const std::string& text)
{
	const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		text.c_str(), static_cast<int>(text.size()), nullptr, 0);
	if (size <= 0) return std::wstring();
	std::wstring result(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(),
		static_cast<int>(text.size()), &result[0], size);
	return result;
}

bool CredentialExists(const std::string& username)
{
	const std::wstring target = Utf8ToWide(
		"net.retropc.pi.XM8.RetroAchievements:" + username);
	PCREDENTIALW credential = nullptr;
	const bool exists = CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0,
		&credential) != FALSE;
	if (credential != nullptr) CredFree(credential);
	return exists;
}

struct Cleanup {
	Xm8Ra::RaPlatformCredentialsStore *store;
	std::string root;
	~Cleanup()
	{
		store->Delete(nullptr);
		Xm8Ra::RemoveRaTree(root);
	}
};

} // namespace

int main()
{
	// CTest's ordinary credential tests use an in-memory backend. This test is
	// intentionally Windows-only and must exercise Credential Manager itself.
	SetEnvironmentVariableW(L"XM8_RA_CREDENTIALS_TEST_MEMORY", nullptr);

	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
	char temporary[MAX_PATH + 1] = {};
	const DWORD temporary_size = GetTempPathA(MAX_PATH, temporary);
	const std::string root = std::string(
		temporary_size > 0 && temporary_size <= MAX_PATH ? temporary : ".\\") +
		"xm8-ra-credential-manager-" + std::to_string(unique);
	const std::string username = "xm8-phase9-test-" + std::to_string(unique);
	const std::string token = "xm8-test-token-" + std::to_string(unique);

	Check(Xm8Ra::EnsureRaDirectoryTree(root), "create isolated test root");
	Xm8Ra::RaPlatformCredentialsStore store(root);
	Cleanup cleanup = { &store, root };

	Check(!CredentialExists(username), "test credential absent before test");
	std::string error;
	Check(store.Save({ username, token }, &error),
		"save token to Windows Credential Manager");
	if (!error.empty()) std::cerr << error << '\n';
	Check(CredentialExists(username), "credential visible through CredReadW");

	Xm8Ra::RaCredentials loaded;
	Check(store.Load(&loaded, &error), "load token from Credential Manager");
	Check(loaded.username == username, "loaded username matches");
	Check(loaded.token == token, "loaded token matches");

	std::ifstream hint(root + "/credentials_user.txt", std::ios::binary);
	const std::string hint_data((std::istreambuf_iterator<char>(hint)),
		std::istreambuf_iterator<char>());
	hint.close();
	Check(hint_data.find(token) == std::string::npos,
		"token is not written to username hint file");

	store.ClearSecret(&loaded);
	Check(loaded.token.empty(), "clear loaded token from application buffer");
	Check(store.Delete(&error), "delete test credential");
	Check(!CredentialExists(username), "credential absent after delete");
	Check(!store.Load(&loaded, &error), "deleted credential cannot be loaded");

	return failures == 0 ? 0 : 1;
}
