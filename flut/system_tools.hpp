#pragma once

#include "flut/system/types.hpp"

#ifdef FLUT_COMP_MSVC
	#include <conio.h>
	#include <shlobj.h>
#endif

namespace flut
{
	char wait_for_key() {
#ifdef FLUT_COMP_MSVC
		return _getch();
#else
		return 0;
#endif
	}

	string get_config_folder()
	{
#ifdef FLUT_COMP_MSVC
		// get the string, convert to single byte string, then free the original string (ugh)
		wchar_t* wcsLocalAppData = 0;
		SHGetKnownFolderPath( FOLDERID_LocalAppData, 0, NULL, &wcsLocalAppData );
		char mbsLocalAppData[ MAX_PATH ];
		size_t dummy;
		wcstombs_s( &dummy, mbsLocalAppData, MAX_PATH, wcsLocalAppData, MAX_PATH );
		CoTaskMemFree(static_cast<void*>( wcsLocalAppData ) );

		return string( mbsLocalAppData );
#else
        string homeDir = std::getenv("HOME");
        return homeDir + "/.config";
#endif
	}

}
