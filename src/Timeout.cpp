#include <iodrivers_base/Timeout.hpp>

using namespace iodrivers_base;

Timeout::Timeout(unsigned int timeout)
    : timeout(timeout) {
    gettimeofday(&start_time, 0);
}

void Timeout::restart() {
    gettimeofday(&start_time, 0);
}

bool Timeout::elapsed() const
{
    return elapsed(timeout);
}

bool Timeout::elapsed(unsigned int timeout) const
{
    timeval current_time;
    gettimeofday(&current_time, 0);
    unsigned int elapsed = 
	(current_time.tv_sec - start_time.tv_sec) * 1000
	+ (static_cast<int>(current_time.tv_usec) -
	   static_cast<int>(start_time.tv_usec)) / 1000;
    return timeout < elapsed;
}

unsigned int Timeout::timeLeft() const
{
    return timeLeft(timeout);
}

unsigned int Timeout::timeLeft(unsigned int timeout) const
{
    timeval current_time;
    gettimeofday(&current_time, 0);
    int elapsed = 
	(current_time.tv_sec - start_time.tv_sec) * 1000
	+ (static_cast<int>(current_time.tv_usec) -
	   static_cast<int>(start_time.tv_usec)) / 1000;
    if ((int)timeout < elapsed)
	return 0;
    return timeout - elapsed;
}



