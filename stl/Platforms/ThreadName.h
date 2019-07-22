// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Containers/StringView.h"
#include <thread>

namespace FGC
{

	//void SetThreadName (const std::thread &, StringView name);
	void SetCurrentThreadName (StringView name);

}	// FGC