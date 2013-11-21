tools\sed.exe -i -e "/<PlatformToolset>v110<\/PlatformToolset>/d" %1
tools\sed.exe -i -e "/^\ \ \ \ <ConfigurationType>StaticLibrary<\/ConfigurationType>/a\ \ \ \ <PlatformToolset>v120<\/PlatformToolset>" %1
tools\sed.exe -i -e "/^\ \ \ \ <ConfigurationType>Application<\/ConfigurationType>/a\ \ \ \ <PlatformToolset>v120<\/PlatformToolset>" %1