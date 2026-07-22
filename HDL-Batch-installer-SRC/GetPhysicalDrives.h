#ifndef GETPHYSICALDRIVES_H
#define GETPHYSICALDRIVES_H
#include <windows.h>
// windows.h definit GetClassInfo comme macro (GetClassInfoA/W), ce qui casse la
// macro RTTI de wxWidgets (wxArtProvider::GetClassInfo) avec un wx build "safe".
// On la neutralise ici puisque ce header n'utilise pas l'API Win32 GetClassInfo.
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#include <iostream>
#include <vector>
#include <string>

struct DiskInfo {
    std::string deviceID;
    std::string caption;
    std::string bustype;
};
std::vector<DiskInfo> GetPhysicalDrives();

#endif // GETPHYSICALDRIVES_H
