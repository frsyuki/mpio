#include <mp/wavy.h>
#include <mp/functional.h>
#include <arpa/inet.h>

using namespace mp::placeholders;

void accepted(int fd, int err)
{
	if(fd < 0) {
		errno = err;
		perror("accept error");
		return;
	}

	try {
		std::cout << "accepted" << std::endl;

		// do something with fd
		close(fd);

	} catch (...) {
		::close(fd);
		return;
	}
}

void connected(int fd, int err)
{
	if(fd < 0) {
		errno = err;
		perror("connect error");
		return;
	}

	try {
		std::cout << "connected" << std::endl;

		// do something with fd

	} catch (...) { }
	::close(fd);
}

int main(void)
{
	mp::wavy::loop lo;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(9090);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	lo.listen(PF_INET, SOCK_STREAM, 0,
			(struct sockaddr*)&addr, sizeof(addr),
			mp::bind(&accepted, _1, _2));

	lo.start(4);  // run with 4 threads

	{
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		lo.connect(PF_INET, SOCK_STREAM, 0,
				(struct sockaddr*)&addr, sizeof(addr),
				0.0, connected);
	}

	usleep(50*1e3);

	lo.end();
	lo.join();
}

