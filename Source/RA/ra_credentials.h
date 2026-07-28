#ifndef XM8_RA_CREDENTIALS_H
#define XM8_RA_CREDENTIALS_H

#include <memory>
#include <string>

namespace Xm8Ra {

struct RaCredentials {
	std::string username;
	std::string token;
};

class RaCredentialsStore {
public:
	virtual ~RaCredentialsStore() = default;

	virtual bool Save(const RaCredentials& credentials, std::string *error) = 0;
	virtual bool Load(RaCredentials *credentials, std::string *error) const = 0;
	virtual bool Delete(std::string *error) = 0;
	virtual void ClearSecret(RaCredentials *credentials) const = 0;
};

class RaPlatformCredentialsStore final : public RaCredentialsStore {
public:
	explicit RaPlatformCredentialsStore(const std::string& ra_root);

	bool Save(const RaCredentials& credentials, std::string *error) override;
	bool Load(RaCredentials *credentials, std::string *error) const override;
	bool Delete(std::string *error) override;
	void ClearSecret(RaCredentials *credentials) const override;

private:
	std::string UsernameHintPath() const;

	std::string ra_root_;
};

std::unique_ptr<RaCredentialsStore> CreatePlatformRaCredentialsStore(
	const std::string& ra_root);

} // namespace Xm8Ra

#endif
