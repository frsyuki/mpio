#include <mp/wavy.h>
#include <mp/functional.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>

using namespace mp::placeholders;

bool timer_handler(int* count, mp::wavy::loop* lo)
{
	std::cout << "timer" << std::endl;

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
	lo.add_timer(0.1, 0.1, mp::bind(
				&timer_handler, &count, &lo));

	lo.run(4);
}

