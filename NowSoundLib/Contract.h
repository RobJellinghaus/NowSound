// The Wazappy project implements a WASAPI-based sound engine for Windows UWP and desktop apps.
// https://github.com/RobJellinghaus/Wazappy
// Licensed under the MIT License.

#pragma once

#include <iostream>
#include <sstream>
#include <string>

namespace NowSound
{
	class Contract
	{
	public:
		static void Requires(bool value)
		{
			Requires(value, L"(no description)");
		}

		static void Assert(bool value)
		{
			Assert(value, L"(no description)");
		}

		static void Requires(bool value, const std::wstring& label)
		{
			Check(value, L"Requires", label);
		}

		static void Assert(bool value, const std::wstring& label)
		{
			Check(value, L"Assert", label);
		}

	private:
		static void Check(bool value, const std::wstring& source, const std::wstring& label)
		{
			if (!value)
			{
				std::wstringstream wstr;

				wstr << source << L" failure: " << label << L"\r\n";
				std::wstring str(wstr.str());
				std::wcerr << str;
				std::wcerr.flush();

				std::abort();
			}
		}
	};
}
