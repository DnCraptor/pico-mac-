# CMakeList.txt: проект CMake для umac_rom_patch; включите исходный код и определения,
# укажите здесь логику для конкретного проекта.
#
cmake_minimum_required (VERSION 3.8)

# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("umac_rom_patch")

# Добавьте источник в исполняемый файл этого проекта.
add_executable (
	umac_rom_patch
	"umac_rom_patch.cpp"
	"umac_rom_patch.h"
	"rom.c"
	"rom.h"
	"machw.h"
)

add_compile_definitions(USE_VGA_RES=1)
add_compile_definitions(DISP_WIDTH=640)
add_compile_definitions(DISP_HEIGHT=480)
add_compile_definitions(UMAC_MEMSIZE=208)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET umac_rom_patch PROPERTY CXX_STANDARD 20)
endif()

# TODO: Добавьте тесты и целевые объекты, если это необходимо.
