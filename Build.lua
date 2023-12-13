-- premake5.lua
workspace "FAT12"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "FAT12-App"

   -- Workspace-wide build options for MSVC
   filter "system:windows"
      buildoptions { "/EHsc", "/Zc:preprocessor", "/Zc:__cplusplus" }

OutputDir = "%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}"

group "FAT12-Core"
	include "FAT12-Core/Build-Core.lua"
group ""

include "FAT12-App/Build-App.lua"