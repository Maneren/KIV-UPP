#pragma once

#include <map>

#include "user.h"

// "svet" ve kterem jsou vsichni uzivatele
class CWorld
{
	private:
		// mapa uzivatelu
		std::map<std::string, std::shared_ptr<CUser>> m_users;

	public:
		CWorld() = default;

		// vytvori uzivatele
		std::shared_ptr<CUser> Create_User(const std::string& username);
		// ziska uzivatele dle jmena
		std::shared_ptr<CUser> Get_User(const std::string& username);

		// nastartuje vlakna uzivatelu
		void Start();
		// pocka na skonceni vlaken uzivatelu
		void Wait();
};

extern CWorld sWorld;
