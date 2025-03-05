#include "world.h"

CWorld sWorld;

std::shared_ptr<CUser> CWorld::Create_User(const std::string& username)
{
	std::shared_ptr<CUser> usr = std::make_shared<CUser>(username);

	m_users[username] = usr;

	return usr;
}

std::shared_ptr<CUser> CWorld::Get_User(const std::string& username)
{
	if (m_users.find(username) != m_users.end()) {
		return m_users[username];
	}
	return nullptr;
}

void CWorld::Start()
{
	for (auto& usr : m_users) {
		usr.second->Start();
	}
}

void CWorld::Wait()
{
	for (auto& usr : m_users) {
		usr.second->Wait();
	}
}
