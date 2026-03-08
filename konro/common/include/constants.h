#ifndef CONSTANTS_H
#define CONSTANTS_H

#if defined(KONRO_RM)
	#define CONFIG_PATH "konro.ini"
#elif defined(KORNO_APPLIB)
	#define CONFIG_PATH "applib.ini"
#elif defined(KONRO_COMMON)

#else
	#error "Either KONRO_RM or KONRO_APPLIB must be defined"
#endif

#endif // CONSTANTS_HPP
