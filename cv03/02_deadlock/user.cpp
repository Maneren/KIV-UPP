#include "user.h"

#include <iostream>
#include <random>

#include "world.h"

namespace
{
	std::random_device rdev;
}

// pomocna funkce pro pojmenovani vlakna - abychom ho pak v debuggeru snadno nasli
#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
void Set_Thread_Name(const std::string& name)
{
	std::wstring wstr(name.begin(), name.end());

	SetThreadDescription(GetCurrentThread(),wstr.c_str());
}
#else
#include <pthread.h>
void Set_Thread_Name(const std::string& name)
{
	pthread_setname_np(pthread_self(), name.c_str());
}
#endif

void CUser::Add_Friend(const std::string& username)
{
	m_friends.push_back(username);
}

void CUser::Pass_Message(const std::string& from, const std::string& message)
{
	m_messageQueue.push_back({ from, message });
}

void CUser::Start()
{
	if (m_thread) {
		return;
	}

	m_running = true;
	m_thread = std::make_unique<std::thread>(&CUser::Thread_Fnc, this);
}

void CUser::Wait()
{
	if (m_thread && m_thread->joinable()) {
		m_thread->join();
	}
}

void CUser::Thread_Fnc()
{
	Set_Thread_Name(m_username);

	std::default_random_engine reng(rdev());
	std::uniform_real_distribution<double> dist_contact(0, 1.0);
	std::uniform_int_distribution<size_t> dist_friend(0, m_friends.size() - 1);

	while (m_running)
	{
		// dokud neni fronta prichozich zprav prazdna...
		while (!m_messageQueue.empty()) {

			// vybereme zpravu
			auto msg = m_messageQueue.front();

			// vypiseme ji na vystup
			std::cout << "From " << msg.from << " to " << m_username << ": " << msg.msg << std::endl;

			// a pokud to neni odpoved, tak pozdravime zpet
			if (msg.msg.substr(0, 5) != "REPLY") {

				// posleme mu odpoved
				auto usr = sWorld.Get_User(msg.from);
				if (usr) {
					usr->Pass_Message(m_username, "REPLY: Hi!");
				}
			}

			// odebereme z fronty
			m_messageQueue.pop_front();
		}

		// hodime kostkou, a pokud to vyjde, tak kontaktujeme nejakeho nahodneho kamarada
		if (dist_contact(reng) < 0.3) {

			auto fr_idx = dist_friend(reng);
			auto usr = sWorld.Get_User(m_friends[fr_idx]);
			if (usr) {
				usr->Pass_Message(m_username, "Hello!");
			}
		}
	}
}
