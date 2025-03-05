#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>

// pocatecni zustatek
constexpr double Initial_Balance = 15000.0;
// ocekavany zustatek na konci
constexpr double Expected_Balance = 15000.0;

// pocet testovacich operaci s uctem
constexpr size_t Number_Of_Tests = 1'000'000;
// pocet vlaken, ktere budou soubezne modifikovat
constexpr size_t Number_Of_Test_Threads = 20;

// rozhrani bankovniho uctu
class IBank_Account
{
	protected:
		// aktualni zustatek
		double m_balance;

	public:
		IBank_Account(double initial) : m_balance(initial) {}

		// ulozeni penez
		virtual void Deposit(double amount) = 0;
		// vyber penez
		virtual bool Withdraw(double amount) = 0;

		// ziskani aktualniho zustatku
		double Get_Balance() const
		{
			return m_balance;
		}
};

// "nebezpecna" implementace bankovniho uctu
class CBank_Account_Unsafe : public IBank_Account
{
	public:
		CBank_Account_Unsafe(double initial) : IBank_Account(initial) {}

		virtual void Deposit(double amount) override
		{
			m_balance += amount;
		}

		virtual bool Withdraw(double amount) override
		{
			if (m_balance < amount)
				return false;

			m_balance -= amount;
			return true;
		}
};

// "bezpecna" implementace bankovniho uctu
class CBank_Account_Mutex : public IBank_Account
{
	private:
		std::mutex m_mtx;

	public:
		CBank_Account_Mutex(double initial) : IBank_Account(initial) {}

		virtual void Deposit(double amount) override
		{
			// lock_guard - v konstruktoru zamkne mutex
			std::lock_guard<std::mutex> guard(m_mtx);

			m_balance += amount;
		} // v destruktoru lock_guard (tady) mutex zase odemkne

		virtual bool Withdraw(double amount) override
		{
			std::lock_guard<std::mutex> guard(m_mtx);

			if (m_balance < amount)
				return false; // + se odemkne mutex (destruktor lock_guard)

			m_balance -= amount;
			return true;
		}
};

// testovaci funkce pro operaci s ucty
void Bank_Account_Test_Fnc(IBank_Account& account)
{
	for (size_t i = 0; i < Number_Of_Tests; i++)
	{
		// vlozime 1000 Kc
		account.Deposit(1000.0);
		// vybereme 1000 Kc
		account.Withdraw(1000.0);
	}
}

int main(int argc, char** argv)
{
	// testovaci ucet - nechte odkomentovany jen ten, ktery chcete vyzkouset
	CBank_Account_Unsafe account(Initial_Balance);
	//CBank_Account_Mutex account(Initial_Balance);

	// zmerime i cas
	auto tp_start = std::chrono::steady_clock::now();

	// zapnuti vlaken
	std::vector<std::unique_ptr<std::thread>> threads(Number_Of_Test_Threads);
	for (size_t i = 0; i < threads.size(); i++) {
		threads[i] = std::make_unique<std::thread>(&Bank_Account_Test_Fnc, std::ref(account));
	}

	// pockani na vlakna, az dokonci cinnost
	for (size_t i = 0; i < threads.size(); i++) {
		threads[i]->join();
	}

	auto tp_end = std::chrono::steady_clock::now();
	std::cout << "Test was running for: " << std::chrono::duration_cast<std::chrono::milliseconds>(tp_end - tp_start).count() << "ms" << std::endl;

	std::cout << "Account balance: " << account.Get_Balance() << std::endl;
	std::cout << "Expected balance: " << Expected_Balance << std::endl;

	// dovolime nejakou malou odchylku (stale pracujeme s doubly, ktere mohou byt z podstaty nepresne)
	if (std::fabs(account.Get_Balance() - Expected_Balance) > 10.0 * std::numeric_limits<double>::epsilon())
		std::cout << "Test FAILED!" << std::endl;
	else
		std::cout << "Test PASSED!" << std::endl;

	return 0;
}
