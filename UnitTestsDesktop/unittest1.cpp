#include "pch.h"
#include "CppUnitTest.h"

#include "Check.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace NowSound;

namespace UnitTestsDesktop
{		
	TEST_CLASS(NowSoundDesktopTests)
	{
	public:
		
		TEST_METHOD(TestCheck)
		{
            // this should be fine
            Check(true);
		}
	};
}