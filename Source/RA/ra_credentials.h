#ifndef XM8_RA_CREDENTIALS_H
#define XM8_RA_CREDENTIALS_H

#include <string>

namespace Xm8Ra {

struct RaCredentials {
	std::string username;
	std::string token;
};

class RaCredentialsStore {
public:
	explicit RaCredentialsStore(const std::string& ra_root);

	bool Save(const RaCredentials& credentials, std::string *error);
	bool Load(RaCredentials *credentials, std::string *error) const;
	bool Delete(std::string *error);
	void ClearSecret(RaCredentials *credentials) const;

private:
	std::string UsernameHintPath() const;

	std::string ra_root_;
};

} // namespace Xm8Ra

#endif
