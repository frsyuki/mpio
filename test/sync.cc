#include <mp/sync.h>
#include <mp/pthread.h>
#include <vector>

struct test {
	test(int num1, int num2) :
		num1(num1), num2(num2) { }

	volatile int num1;
	volatile int num2;
};

void thread_main(mp::sync<test>* obj)
{
	for(int i=0; i < 20; ++i) {
		mp::sync<test>::ref ref(*obj);
		ref->num1++;
		ref->num1--;
		std::cout << (ref->num1 + ref->num2);
	}
}

int main(void)
{
	mp::sync<test> obj(0, 0);

	std::vector<mp::pthread_thread> threads(4);
	for(int i=0; i < 4; ++i) {
		threads[i].run(mp::bind(&thread_main, &obj));
	}

	for(int i=0; i < 4; ++i) {
		threads[i].join();
	}

	std::cout << std::endl;
}

