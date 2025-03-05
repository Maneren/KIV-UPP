#include <iostream>

#include "world.h"

int main(int argc, char** argv)
{
	// vytvorime par pratel
	auto john = sWorld.Create_User("john");
	auto jim = sWorld.Create_User("jim");
	auto jack = sWorld.Create_User("jack");
	auto joe = sWorld.Create_User("joe");

	// vsechny spratelime

	john->Add_Friend("jim");
	john->Add_Friend("jack");
	john->Add_Friend("joe");

	jim->Add_Friend("john");
	jim->Add_Friend("jack");
	jim->Add_Friend("joe");

	jack->Add_Friend("john");
	jack->Add_Friend("jim");
	jack->Add_Friend("joe");

	joe->Add_Friend("john");
	joe->Add_Friend("jim");
	joe->Add_Friend("jack");

	// a nechame je mezi sebou povidat
	sWorld.Start();

	// a pockame az se ukonci
	sWorld.Wait();

	return 0;
}
