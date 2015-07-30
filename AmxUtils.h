#pragma once

#include <string>
#include "amx/amx.h"

class AmxUtils
{	
	public:
		static std::string amx_GetStdString(AMX *amx, cell *param);
};