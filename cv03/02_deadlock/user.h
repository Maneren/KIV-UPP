#pragma once

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <list>

// struktura zpravy
struct TMsg_Record {
	std::string from;
	std::string msg;
};

// uzivatel systemu
class CUser
{
	private:
		// uzivatelske jmeno
		std::string m_username;

		// seznam "pratel" (komu se bude snazit poslat zpravu)
		std::vector<std::string> m_friends;

		// fronta zprav
		std::list<TMsg_Record> m_messageQueue;

		// bezime?
		bool m_running = false;
		// vlako uzivatele
		std::unique_ptr<std::thread> m_thread;

		// mutex - budeme potrebovat
		//std::mutex m_queueMtx;

	private:
		// metoda vlakna
		void Thread_Fnc();

	public:
		CUser(const std::string& username) : m_username(username) {}

		// prida pritele do seznamu
		void Add_Friend(const std::string& username);

		// preda zpravu tomuto uzivateli od jineho
		void Pass_Message(const std::string& from, const std::string& message);

		// nastartuje vlakno
		void Start();

		// pocka na skonceni vlakna
		void Wait();
};
