#include "examples.h"
#include <atomic>
#include <iostream>
#include <thread>


#include "stlab/concurrency/channel.hpp"
#include "stlab/concurrency/default_executor.hpp"

using namespace std;
using namespace stlab;

void test_channel() {
	sender<int> send;
	receiver<int> receive;

	tie(send, receive) = channel<int>(default_executor);

	std::atomic_bool done{ false };

	auto hold = receive | [&_done = done](int x) {
		cout << x << '\n';
		if(x == 3) _done = true;
	};

	// It is necessary to mark the receiver side as ready, when all connections are
	// established
	receive.set_ready();

	send(1);
	send(2);
	send(3);

	// Waiting just for illustrational purpose
	while (!done.load()) {
		this_thread::sleep_for(chrono::milliseconds(1));
	}
}


void test_join() {
	sender<int> send1, send2;
	receiver<int> receive1, receive2;

	tie(send1, receive1) = channel<int>(default_executor);
	tie(send2, receive2) = channel<int>(default_executor);

	std::atomic_bool done{ false };

	auto joined = join(default_executor,
		[](int x, int y) { return x + y; }, // x will get 1, y will get 2
		receive1,
		receive2)
		| [&_done = done](int x) {
		cout << x << '\n';
		_done = true;
	};

	receive1.set_ready();
	receive2.set_ready();

	send1(1);
	send2(2);

	// Waiting just for illustrational purpose
	while (!done) {
		this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

struct sum_2
{
	process_state_scheduled _state = await_forever;
	int _counter = 0;
	int _sum = 0;

	void await(int n) {
		_sum += n;
		++_counter;
		if (_counter == 2) _state = yield_immediate;
	}

	int yield() {
		_state = await_forever;
		auto result = _sum;
		_sum = 0;
		_counter = 0;
		return result;
	}

	auto state() const { return _state; }
};

void test_await() {

	sender<int> send;
	receiver<int> receive;

	tie(send, receive) = channel<int>(default_executor);

	std::atomic_int r = 0;
	/*
	yield from sum_2 is feed to lambda
	*/
	auto hold = receive
		| sum_2()
		| [&_r = r](int x) { _r = x; };

	receive.set_ready();

	send(1);
	send(2);

	// Waiting just for illustrational purpose
	while (r == 0) {
		this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	cout << "The sum is " << r.load() << '\n';
	

}


struct nop_process
{
	process_state_scheduled _state = await_forever;

	int _v = 0;

	~nop_process() { std::cout << "nop_process::~nop_process()\n"; }

	void await(int x) {
		_v = x;
		_state = yield_immediate;
	}

	int yield() {
		_state = await_forever;
		return _v;
	}

	auto state() const { return _state; }
};
void test_close() {
	sender<int> send;
	receiver<int> receive;
	std::tie(send, receive) = channel<int>(default_executor);
	std::atomic_bool done{ false };

	auto result = receive |
		nop_process{} |
		[&_done = done](int v) {
		std::cout << "Received " << v << '\n';
		_done = (v > 2);
	};

	receive.set_ready();
	std::cout << "Start sending...\n";
	send(1);
	send(2);
	send(3);
	std::cout << "Closing channel\n";
	send.close(); //it makes call to destructor of nop_process

	while (!done) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	std::cout << "Waited till the end.\n";
}


/*
This process adds all values until a zero is passed as value.
Then it will yield the result and start over again.
*/

struct adder
{
	int _sum = 0;
	process_state_scheduled _state = await_forever;

	void await(int x) {
		_sum += x;
		if (x == 0) {
			_state = yield_immediate;
		}
	}

	int yield() {
		int result = _sum;
		_sum = 0;
		_state = await_forever;
		return result;
	}

	auto state() const { return _state; }
};

void test_process() {
	sender<int> send;
	receiver<int> receiver;
	std::tie(send, receiver) = channel<int>(default_executor);

	std::atomic_bool done{ false };

	auto calculator = receiver |
		adder{} |
		[&_done = done](int x) { std::cout << x << '\n';
	_done = true;
	};

	receiver.set_ready();

	send(1);
	send(2);
	send(3);
	send(0);

	// Waiting just for illustrational purpose
	while (!done) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}


#include "stlab/concurrency/immediate_executor.hpp"

void test_anonated_process() {
	sender<int> send;
	receiver<int> receive;

	std::tie(send, receive) = channel<int>(default_executor);

	std::atomic_int v{ 0 };

	// The code demonstrates how a process can be annotated
	//Creates a new receiver, attaches the given process as downstream to it
	auto result = receive
		| buffer_size{ 3 } &[](int x) { return x * 2; }
	| [](int x) { return x * 2; } &buffer_size{ 2 }
	| buffer_size{ 3 } &executor{ default_executor } &[](int x) { return x * 2; }

	| executor{ default_executor } &[](int x) { return x + 1; }
	| [](int x) { return x + 1; } &executor{ immediate_executor }
	| executor{ default_executor } &buffer_size{ 3 } &[](int x) { return x * 2; }

	| [](int x) { return x + 1; } &executor{ default_executor } &buffer_size{ 3 }
	| [](int x) { return x * 2; } &buffer_size{ 3 } &executor{ immediate_executor }

	| [&v](int x) { v = x; };

	receive.set_ready();

	send(1);

	while (v == 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	std::cout << "v = " << v << std::endl;
}


void attaching_to_reciever() {
	sender<int> send;
	receiver<int> receive;

	tie(send, receive) = channel<int>(default_executor);

	std::atomic_bool done{ false };

	auto hold = receive | [&_done = done](int x) {
		cout << "hold x = "<< x << '\n';
		_done = true;
	};

	auto hold2 = receive | [](int x) {
		cout << " in hold 2 = " << x << '\n';
	};

	// It is necessary to mark the receiver side as ready, when all connections are
	// established
	receive.set_ready();

	send(42);

	// Waiting just for illustrational purpose
	while (!done.load()) {
		this_thread::sleep_for(chrono::milliseconds(10));
	}
}


void test_executor() {
	sender<int> send;
	receiver<int> receive;

	tie(send, receive) = channel<int>(default_executor);

	std::atomic_int v{ 0 };

	// The process times_two will be executed immediately
	// The order of the process and executor is not relevant so calling
	// times_two & executor{ immediate_executor } would be equivalent
	auto result = receive
		| executor{ immediate_executor } &[](int x) { return x * 2; }
	| [&v](int x) { v = x; };

	receive.set_ready();

	send(1);

	// Waiting just for illustrational purpose
	while (v == 0) {
		this_thread::sleep_for(chrono::milliseconds(1));
	}

	
}



#include "stlab/concurrency/future.hpp"

void test_recovery() {
	auto x = async(default_executor, [] {
		throw runtime_error("Vogons did arrive!");
		return 42;
	});

	auto r = x.recover([](future<int> f) {
		try {
			auto answer = f.get_try().value();
			cout << "The answer is " << answer << '\n';
		}
		catch (const exception& ex) {
			cout << "The error \"" << ex.what() << "\" happened!\n";
		}
	});

	// Waiting just for illustrational purpose
	while (!r.get_try()) { this_thread::sleep_for(chrono::milliseconds(1)); }
}

void then_example() {
	auto x = async(default_executor, [] { return 42; });

	auto y = x.then([](int x) { printf("Result %d \n", x); });

	// Waiting just for illustrational purpose
	while (!y.get_try()) { this_thread::sleep_for(chrono::milliseconds(1)); }
}


void then_example2() {
	auto x = async(default_executor, [] { return 42; });

	auto c1 = x.then([](int x) { printf("Split A %d \n", x); });
	auto c2 = x.then([](int x) { printf("Split B %d \n", x); });

	// Waiting just for illustrational purpose
	while (!c1.get_try()) { this_thread::sleep_for(chrono::milliseconds(1)); }
	while (!c2.get_try()) { this_thread::sleep_for(chrono::milliseconds(1)); }
}


void test_packaged_task() {
	auto p = package<int(int)>(immediate_executor, [](int x) {
		cout << "task thread id = " << this_thread::get_id() << "\n";
		return x + x; });
	auto packagedTask = p.first;
	auto f = p.second;

	packagedTask(21);

	// Waiting just for illustrational purpose
	while (!f.get_try()) { this_thread::sleep_for(chrono::milliseconds(1)); }
	cout << "main thread id = " << this_thread::get_id() << "\n";
	cout << "The answer is " << f.get_try().value() << "\n";
}

void test_when_all() {
	auto argument1 = async(default_executor, [] { 
		cout << "argument 1 id = " << this_thread::get_id() << "\n";
		return 42; });
	auto argument2 = async(default_executor, [] { 
		cout << "argument2 id = " << this_thread::get_id() << "\n";
		return string("The answer is"); });

	auto result = when_all(default_executor, [](int answer, std::string text) {
		cout << "when all id = " << this_thread::get_id() << "\n";
		cout << text << " " << answer << '\n';
	}, argument1, argument2);

	cout << "main id = " << this_thread::get_id() << "\n";
	this_thread::sleep_for(chrono::milliseconds(10000));
	// Waiting just for illustrational purpose
	//while (!result.get_try()) { this_thread::sleep_for(chrono::milliseconds(1)); }
}