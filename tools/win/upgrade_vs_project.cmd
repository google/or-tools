tools\win\sed.exe -i -e "/<PlatformToolset>/d" %1
tools\win\sed.exe -i -e "/^\ \ \ \ <ConfigurationType>StaticLibrary<\/ConfigurationType>/a\ \ \ \ <PlatformToolset>%2<\/PlatformToolset>" %1
tools\win\sed.exe -i -e "/^\ \ \ \ <ConfigurationType>Application<\/ConfigurationType>/a\ \ \ \ <PlatformToolset>%2<\/PlatformToolset>" %1
