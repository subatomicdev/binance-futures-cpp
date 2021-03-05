#ifndef BFCPP_TEST_COMMON_H
#define BFCPP_TEST_COMMON_H

#include <Futures.hpp>

using namespace bfcpp;
using namespace std::chrono_literals;

class BfcppTest
{

public:
	virtual ~BfcppTest()
	{
	}

	virtual void run() = 0;

protected:
	BfcppTest(ApiAccess access) : m_access(access)
	{
	}


protected:
	ApiAccess m_access;
};


#endif

