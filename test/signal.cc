#include <mp/signal.h>
#include <mp/functional.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>

using namespace mp::placeholders;

bool signal_handler(int signo, int* count)
{
	std::cout << "signal" << std::endl;

	if(++(*count) >= 3) {
		return false;
	}

	return true;
}

int main(void)
{
	int count = 0;

	mp::pthread_signal th(
			mp::sigset().add(SIGUSR1),
			mp::bind(&signal_handler, _1, &count));

	pid_t pid = getpid();

	usleep(50*1e3);
	kill(pid, SIGUSR1);
	usleep(50*1e3);
	kill(pid, SIGUSR1);
	usleep(50*1e3);
	kill(pid, SIGUSR1);

	th.join();
}

