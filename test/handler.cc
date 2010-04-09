#include <mp/wavy.h>
#include <mp/signal.h>
#include <mp/functional.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>

using namespace mp::placeholders;

class handler : public mp::wavy::handler {
public:
	handler(int fd, mp::wavy::loop* lo) :
		mp::wavy::handler(fd),
		m_lo(lo),
		m_count(0) { }

	void on_read(mp::wavy::event& e)
	{
		char buf[512];
		ssize_t rl = read(fd(), buf, sizeof(buf));
		if(rl <= 0) {
			if(rl == 0) {
				throw mp::system_error(errno, "connection closed");
			}
			if(errno == EINTR || errno == EAGAIN) { return; }
		}

		std::cout << "read "<<rl<<" bytes: ";
		std::cout.write(buf, rl);
		std::cout << std::endl;

		m_lo->end();
	}

private:
	mp::wavy::loop* m_lo;
	int m_count;
};

void reader_main(int rpipe)
{
	mp::wavy::loop lo;

	lo.add_handler<handler>(rpipe, &lo);

	lo.run(4);
}

void writer_main(int wpipe)
{
	mp::wavy::loop lo;

	for(int i=0; i < 15; ++i) {
		lo.write(wpipe, "test", 4);
	}

	lo.start(4);

	wait(NULL);

	lo.end();
}

int main(void)
{
	int pair[2];
	pipe(pair);

	pid_t pid = fork();
	if(pid == 0) {
		reader_main(pair[0]);
		exit(0);
	}

	writer_main(pair[1]);
}

