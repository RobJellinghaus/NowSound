#pragma once

// NowSound library by Rob Jellinghaus, https://github.com/RobJellinghaus/NowSound
// Licensed under the MIT license

#include "stdafx.h"

#include "Check.h"

namespace NowSound
{
	template <typename T>
	class Option
	{
	private:
		const T _value;
		const bool _hasValue;

	public:
		T Value()
		{
			Check(HasValue());
			return _value;
		}

		bool HasValue() { return _hasValue; }

		Option(T value) : _value{ value }, _hasValue{ true }
		{ }

		Option() : _value{}, _hasValue { false }
		{ }
	};
}