#ifndef VERSION_H
#define VERSION_H

namespace AutoVersion{
	
	//Date Version Types
	static const char DATE[] = "22";
	static const char MONTH[] = "07";
	static const char YEAR[] = "2026";
	static const char UBUNTU_VERSION_STYLE[] =  "26.03";
	
	//Software Status
	static const char STATUS[] =  "Beta";
	static const char STATUS_SHORT[] =  "b";
	
	//Standard Version Type
	static const long MAJOR  = 3;
	static const long MINOR  = 8;
	static const long BUILD  = 0;
	static const long REVISION  = 14;
	
	//Miscellaneous Version Types
	static const long BUILDS_COUNT  = 1;
	#define RC_FILEVERSION 3,8,0,14
	#define RC_FILEVERSION_STRING "3, 8, 0, 14\0"
	static const char FULLVERSION_STRING [] = "3.8.0.14";
	
	//These values are to keep track of your versioning state, don't modify them.
	static const long BUILD_HISTORY  = 99;
	

}
#endif //VERSION_H
