#include <iodrivers_base/Timeout.hpp>

using namespace iodrivers_base;

Timeout::Timeout(base::Time timeout)
    : timeout(timeout)
    , start_time(base::Time::now())
{
}

void Timeout::restart()
{
    start_time = base::Time::now();
}

bool Timeout::elapsed() const
{
    return elapsed(timeout);
}

bool Timeout::elapsed(base::Time timeout) const
{
    return timeout <= (base::Time::now() - start_time);
}

base::Time Timeout::remaining() const
{
    return remaining(timeout);
}

base::Time Timeout::remaining(base::Time timeout) const
{
    base::Time elapsed = base::Time::now() - start_time;
    if (timeout < elapsed)
        return base::Time();
    else
        return (timeout - elapsed);
}

Timeout::Timeout(unsigned int timeout)
    : timeout(base::Time::fromMilliseconds(timeout))
    , start_time(base::Time::now())
{
}

unsigned int Timeout::timeLeft() const
{
    return remaining(timeout).toMilliseconds();
}

unsigned int Timeout::timeLeft(unsigned int timeout) const
{
    return remaining(base::Time::fromMilliseconds(timeout)).toMilliseconds();
}

bool Timeout::elapsed(unsigned int timeout) const
{
    return elapsed(base::Time::fromMilliseconds(timeout));
}

