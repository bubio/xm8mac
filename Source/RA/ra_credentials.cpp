#include "ra_credentials.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#elif defined(__linux__) && !defined(__ANDROID__) && defined(XM8_RA_HAS_LIBSECRET)
#include <glib.h>
#include <libsecret/secret.h>
#endif

namespace Xm8Ra {
namespace {

constexpr const char *kService = "net.retropc.pi.XM8.RetroAchievements";
constexpr size_t kMaxUsernameBytes = 256;
constexpr size_t kMaxTokenBytes = 4096;

std::string JoinPath(const std::string& dir, const char *name)
{
	if (!dir.empty() && dir.back() == '/') {
		return dir + name;
	}
	return dir + "/" + name;
}

bool SetError(std::string *error, const char *message)
{
	if (error != nullptr) {
		*error = message;
	}
	return false;
}

bool ValidateCredentials(const RaCredentials& credentials, std::string *error)
{
	if (credentials.username.empty()) {
		return SetError(error, "RA username is empty");
	}
	if (credentials.username.size() > kMaxUsernameBytes) {
		return SetError(error, "RA username is too long");
	}
	if (credentials.token.empty()) {
		return SetError(error, "RA token is empty");
	}
	if (credentials.token.size() > kMaxTokenBytes) {
		return SetError(error, "RA token is too long");
	}
	return true;
}

bool ValidateUsername(const std::string& username, std::string *error)
{
	if (username.empty()) {
		return SetError(error, "RA username is not stored");
	}
	if (username.size() > kMaxUsernameBytes) {
		return SetError(error, "RA username is too long");
	}
	return true;
}

bool IsTestMemoryBackendEnabled()
{
	const char *value = std::getenv("XM8_RA_CREDENTIALS_TEST_MEMORY");
	return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

std::mutex& TestCredentialsMutex()
{
	static std::mutex mutex;
	return mutex;
}

std::map<std::string, std::string>& TestCredentials()
{
	static std::map<std::string, std::string> credentials;
	return credentials;
}

std::string CredentialKey(const std::string& username)
{
	return std::string(kService) + ":" + username;
}

bool SaveTestToken(const std::string& username, const std::string& token,
	std::string *)
{
	std::lock_guard<std::mutex> lock(TestCredentialsMutex());
	TestCredentials()[CredentialKey(username)] = token;
	return true;
}

bool LoadTestToken(const std::string& username, std::string *token,
	std::string *error)
{
	std::lock_guard<std::mutex> lock(TestCredentialsMutex());
	const auto it = TestCredentials().find(CredentialKey(username));
	if (it == TestCredentials().end()) {
		return SetError(error, "RA token is not stored");
	}
	*token = it->second;
	return true;
}

bool DeleteTestToken(const std::string& username, std::string *)
{
	std::lock_guard<std::mutex> lock(TestCredentialsMutex());
	TestCredentials().erase(CredentialKey(username));
	return true;
}

bool WriteUsernameHint(const std::string& path, const std::string& username,
	std::string *error)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		return SetError(error, "failed to save RA username");
	}
	stream.write(username.data(), static_cast<std::streamsize>(username.size()));
	stream.put('\n');
	if (!stream.good()) {
		return SetError(error, "failed to write RA username");
	}
	return true;
}

bool ReadUsernameHint(const std::string& path, std::string *username,
	std::string *error)
{
	if (username == nullptr) {
		return false;
	}
	username->clear();
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open()) {
		return SetError(error, "RA username is not stored");
	}
	std::getline(stream, *username);
	if (!stream.good() && !stream.eof()) {
		username->clear();
		return SetError(error, "failed to read RA username");
	}
	if (!username->empty() && username->back() == '\r') {
		username->pop_back();
	}
	return ValidateUsername(*username, error);
}

#ifdef _WIN32
std::wstring Utf8ToWide(const std::string& text)
{
	if (text.empty()) {
		return std::wstring();
	}
	const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		text.c_str(), static_cast<int>(text.size()), nullptr, 0);
	if (size <= 0) {
		return std::wstring();
	}
	std::wstring wide(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(),
		static_cast<int>(text.size()), &wide[0], size);
	return wide;
}
#endif

#if defined(__APPLE__)
const void *DataProtectionKeychainKey()
{
	if (__builtin_available(macOS 10.15, *)) {
		return kSecUseDataProtectionKeychain;
	}
	return nullptr;
}

CFDictionaryRef CreateKeychainQuery(const void **keys, const void **values,
	size_t count, bool data_protection)
{
	std::vector<const void *> query_keys(keys, keys + count);
	std::vector<const void *> query_values(values, values + count);
	const void *data_protection_key = data_protection ?
		DataProtectionKeychainKey() : nullptr;
	if (data_protection_key != nullptr) {
		query_keys.push_back(data_protection_key);
		query_values.push_back(kCFBooleanTrue);
	}
	return CFDictionaryCreate(kCFAllocatorDefault, query_keys.data(),
		query_values.data(), static_cast<CFIndex>(query_keys.size()),
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

void DeleteMacTokenItem(CFStringRef service, CFStringRef account,
	bool data_protection)
{
	const void *delete_keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
	const void *delete_values[] = { kSecClassGenericPassword, service, account };
	CFDictionaryRef delete_query = CreateKeychainQuery(delete_keys,
		delete_values, 3, data_protection);
	if (delete_query != nullptr) {
		SecItemDelete(delete_query);
		CFRelease(delete_query);
	}
}
#endif

bool SavePlatformToken(const std::string& username, const std::string& token,
	std::string *error)
{
	if (IsTestMemoryBackendEnabled()) {
		return SaveTestToken(username, token, error);
	}

#ifdef _WIN32
	const std::wstring target = Utf8ToWide(CredentialKey(username));
	const std::wstring user = Utf8ToWide(username);
	if (target.empty() || user.empty() || token.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
		return SetError(error, "RA credential cannot be encoded");
	}

	CREDENTIALW credential = {};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = const_cast<LPWSTR>(target.c_str());
	credential.UserName = const_cast<LPWSTR>(user.c_str());
	credential.CredentialBlob =
		reinterpret_cast<LPBYTE>(const_cast<char *>(token.data()));
	credential.CredentialBlobSize = static_cast<DWORD>(token.size());
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	if (CredWriteW(&credential, 0) == FALSE) {
		return SetError(error, "failed to save RA token");
	}
	return true;
#elif defined(__APPLE__)
	CFDataRef secret = CFDataCreate(kCFAllocatorDefault,
		reinterpret_cast<const UInt8 *>(token.data()),
		static_cast<CFIndex>(token.size()));
	CFStringRef service = CFStringCreateWithCString(kCFAllocatorDefault,
		kService, kCFStringEncodingUTF8);
	CFStringRef account = CFStringCreateWithCString(kCFAllocatorDefault,
		username.c_str(), kCFStringEncodingUTF8);
	if (secret == nullptr || service == nullptr || account == nullptr) {
		if (secret != nullptr) CFRelease(secret);
		if (service != nullptr) CFRelease(service);
		if (account != nullptr) CFRelease(account);
		return SetError(error, "failed to prepare RA token");
	}

	DeleteMacTokenItem(service, account, false);
	DeleteMacTokenItem(service, account, true);

	const void *add_keys[] = {
		kSecClass,
		kSecAttrService,
		kSecAttrAccount,
		kSecValueData,
		kSecAttrAccessible
	};
	const void *add_values[] = {
		kSecClassGenericPassword,
		service,
		account,
		secret,
		kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
	};
	CFDictionaryRef add_query = CreateKeychainQuery(add_keys, add_values, 5,
		true);
	const OSStatus status = add_query == nullptr ? errSecParam :
		SecItemAdd(add_query, nullptr);
	if (add_query != nullptr) CFRelease(add_query);
	CFRelease(secret);
	CFRelease(service);
	CFRelease(account);
	if (status != errSecSuccess) {
		return SetError(error, "failed to save RA token");
	}
	return true;
#elif defined(__linux__) && !defined(__ANDROID__) && defined(XM8_RA_HAS_LIBSECRET)
	static const SecretSchema schema = {
		"net.retropc.pi.XM8.RetroAchievements",
		SECRET_SCHEMA_NONE,
		{
			{ "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
			{ "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
			{ nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING }
		}
	};
	GError *gerror = nullptr;
	const gboolean ok = secret_password_store_sync(&schema,
		SECRET_COLLECTION_DEFAULT, "XM8 RetroAchievements", token.c_str(),
		nullptr, &gerror, "service", kService, "account", username.c_str(),
		nullptr);
	if (!ok) {
		if (gerror != nullptr) {
			g_error_free(gerror);
		}
		return SetError(error, "failed to save RA token");
	}
	return true;
#else
	return SetError(error, "secure RA credential storage is unavailable");
#endif
}

bool LoadPlatformToken(const std::string& username, std::string *token,
	std::string *error)
{
	if (token == nullptr) {
		return false;
	}
	token->clear();
	if (IsTestMemoryBackendEnabled()) {
		return LoadTestToken(username, token, error);
	}

#ifdef _WIN32
	const std::wstring target = Utf8ToWide(CredentialKey(username));
	if (target.empty()) {
		return SetError(error, "RA credential cannot be encoded");
	}
	PCREDENTIALW credential = nullptr;
	if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential) == FALSE) {
		return SetError(error, "RA token is not stored");
	}
	token->assign(reinterpret_cast<const char *>(credential->CredentialBlob),
		reinterpret_cast<const char *>(credential->CredentialBlob) +
		credential->CredentialBlobSize);
	CredFree(credential);
	return !token->empty() || SetError(error, "RA token is empty");
#elif defined(__APPLE__)
	CFStringRef service = CFStringCreateWithCString(kCFAllocatorDefault,
		kService, kCFStringEncodingUTF8);
	CFStringRef account = CFStringCreateWithCString(kCFAllocatorDefault,
		username.c_str(), kCFStringEncodingUTF8);
	if (service == nullptr || account == nullptr) {
		if (service != nullptr) CFRelease(service);
		if (account != nullptr) CFRelease(account);
		return SetError(error, "failed to prepare RA token lookup");
	}

	const void *keys[] = {
		kSecClass,
		kSecAttrService,
		kSecAttrAccount,
		kSecReturnData,
		kSecMatchLimit
	};
	const void *values[] = {
		kSecClassGenericPassword,
		service,
		account,
		kCFBooleanTrue,
		kSecMatchLimitOne
	};
	CFDictionaryRef query = CreateKeychainQuery(keys, values, 5, true);
	CFTypeRef result = nullptr;
	const OSStatus status = query == nullptr ? errSecParam :
		SecItemCopyMatching(query, &result);
	if (query != nullptr) CFRelease(query);
	CFRelease(service);
	CFRelease(account);
	if (status != errSecSuccess || result == nullptr ||
		CFGetTypeID(result) != CFDataGetTypeID()) {
		if (result != nullptr) CFRelease(result);
		return SetError(error, "RA token is not stored");
	}
	CFDataRef data = static_cast<CFDataRef>(result);
	const UInt8 *bytes = CFDataGetBytePtr(data);
	const CFIndex size = CFDataGetLength(data);
	token->assign(reinterpret_cast<const char *>(bytes),
		reinterpret_cast<const char *>(bytes) + size);
	CFRelease(result);
	return !token->empty() || SetError(error, "RA token is empty");
#elif defined(__linux__) && !defined(__ANDROID__) && defined(XM8_RA_HAS_LIBSECRET)
	static const SecretSchema schema = {
		"net.retropc.pi.XM8.RetroAchievements",
		SECRET_SCHEMA_NONE,
		{
			{ "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
			{ "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
			{ nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING }
		}
	};
	GError *gerror = nullptr;
	gchar *secret = secret_password_lookup_sync(&schema, nullptr, &gerror,
		"service", kService, "account", username.c_str(), nullptr);
	if (gerror != nullptr) {
		g_error_free(gerror);
	}
	if (secret == nullptr) {
		return SetError(error, "RA token is not stored");
	}
	token->assign(secret);
	secret_password_free(secret);
	return !token->empty() || SetError(error, "RA token is empty");
#else
	return SetError(error, "secure RA credential storage is unavailable");
#endif
}

bool DeletePlatformToken(const std::string& username, std::string *error)
{
	if (username.empty()) {
		return true;
	}
	if (IsTestMemoryBackendEnabled()) {
		return DeleteTestToken(username, error);
	}

#ifdef _WIN32
	const std::wstring target = Utf8ToWide(CredentialKey(username));
	if (target.empty()) {
		return SetError(error, "RA credential cannot be encoded");
	}
	if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) == FALSE &&
		GetLastError() != ERROR_NOT_FOUND) {
		return SetError(error, "failed to delete RA token");
	}
	return true;
#elif defined(__APPLE__)
	CFStringRef service = CFStringCreateWithCString(kCFAllocatorDefault,
		kService, kCFStringEncodingUTF8);
	CFStringRef account = CFStringCreateWithCString(kCFAllocatorDefault,
		username.c_str(), kCFStringEncodingUTF8);
	if (service == nullptr || account == nullptr) {
		if (service != nullptr) CFRelease(service);
		if (account != nullptr) CFRelease(account);
		return SetError(error, "failed to prepare RA token deletion");
	}
	const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount };
	const void *values[] = { kSecClassGenericPassword, service, account };
	CFDictionaryRef query = CreateKeychainQuery(keys, values, 3, true);
	const OSStatus data_protection_status = query == nullptr ? errSecParam :
		SecItemDelete(query);
	if (query != nullptr) CFRelease(query);
	query = CreateKeychainQuery(keys, values, 3, false);
	const OSStatus legacy_status = query == nullptr ? errSecParam :
		SecItemDelete(query);
	if (query != nullptr) CFRelease(query);
	CFRelease(service);
	CFRelease(account);
	if ((data_protection_status != errSecSuccess &&
		data_protection_status != errSecItemNotFound) ||
		(legacy_status != errSecSuccess &&
		legacy_status != errSecItemNotFound)) {
		return SetError(error, "failed to delete RA token");
	}
	return true;
#elif defined(__linux__) && !defined(__ANDROID__) && defined(XM8_RA_HAS_LIBSECRET)
	static const SecretSchema schema = {
		"net.retropc.pi.XM8.RetroAchievements",
		SECRET_SCHEMA_NONE,
		{
			{ "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
			{ "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
			{ nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING }
		}
	};
	GError *gerror = nullptr;
	secret_password_clear_sync(&schema, nullptr, &gerror, "service", kService,
		"account", username.c_str(), nullptr);
	if (gerror != nullptr) {
		g_error_free(gerror);
		return SetError(error, "failed to delete RA token");
	}
	return true;
#else
	return true;
#endif
}

} // namespace

RaCredentialsStore::RaCredentialsStore(const std::string& ra_root)
	: ra_root_(ra_root)
{
}

std::string RaCredentialsStore::UsernameHintPath() const
{
	return JoinPath(ra_root_, "credentials_user.txt");
}

bool RaCredentialsStore::Save(const RaCredentials& credentials,
	std::string *error)
{
	if (!ValidateCredentials(credentials, error)) {
		return false;
	}
	if (!SavePlatformToken(credentials.username, credentials.token, error)) {
		return false;
	}
	if (!WriteUsernameHint(UsernameHintPath(), credentials.username, error)) {
		std::string ignored_error;
		DeletePlatformToken(credentials.username, &ignored_error);
		return false;
	}
	return true;
}

bool RaCredentialsStore::Load(RaCredentials *credentials,
	std::string *error) const
{
	if (credentials == nullptr) {
		return false;
	}
	credentials->username.clear();
	credentials->token.clear();

	std::string username;
	if (!ReadUsernameHint(UsernameHintPath(), &username, error)) {
		return false;
	}
	std::string token;
	if (!LoadPlatformToken(username, &token, error)) {
		return false;
	}
	credentials->username = username;
	credentials->token = token;
	return true;
}

bool RaCredentialsStore::Delete(std::string *error)
{
	std::string username;
	std::string ignored_error;
	ReadUsernameHint(UsernameHintPath(), &username, &ignored_error);
	if (!DeletePlatformToken(username, error)) {
		return false;
	}
	if (std::remove(UsernameHintPath().c_str()) != 0 && errno != ENOENT) {
		return SetError(error, "failed to delete RA username");
	}
	return true;
}

void RaCredentialsStore::ClearSecret(RaCredentials *credentials) const
{
	if (credentials == nullptr) {
		return;
	}
	volatile char *token = credentials->token.empty() ? nullptr :
		&credentials->token[0];
	for (size_t i = 0; i < credentials->token.size(); i++) {
		token[i] = 0;
	}
	credentials->token.clear();
}

} // namespace Xm8Ra
