#include <mp/wavy.h>
#include <mp/functional.h>
#include <mp/signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>

using namespace mp::placeholders;

bool signal_handler(int* count, mp::wavy::loop* lo)
{
	std::cout << "signal" << std::endl;

	if(++(*count) >= 3) {
		lo->end();
		return false;
	}

	return true;
}

int main(void)
{
	mp::wavy::loop lo;

	int count = 0;

	// add signal handler before starting any other threads.
	lo.add_signal(SIGUSR1, mp::bind(
				&signal_handler, &count, &lo));

	lo.start(3);

	pid_t pid = getpid();

	usleep(50*1e3);
	kill(pid, SIGUSR1);
	usleep(50*1e3);
	kill(pid, SIGUSR1);
	usleep(50*1e3);
	kill(pid, SIGUSR1);

	lo.join();
}

