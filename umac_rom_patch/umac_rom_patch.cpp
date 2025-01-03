// umac_rom_patch.cpp: определяет точку входа для приложения.
//

#include "umac_rom_patch.h"
#include <fstream>

extern "C" {
	#include "rom.h"
}

using namespace std;

int main()
{
	uint8_t buf[128 << 10] = { 0 };
	std::ifstream file("C:\\Pico\\murmulator\\Cross\\pico-mac\\1986-03 - 4D1F8172 - MacPlus v3.ROM", std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	if (file.read((char*)buf, sizeof(buf))) {
		if (!rom_patch(buf)) {
			char b[128];
			for (size_t i = 0; i < sizeof(buf); i += 16) {
				snprintf(b, 128, "0x%02X,0x%02X,0x%02X,0x%02X, 0x%02X,0x%02X,0x%02X,0x%02X,  0x%02X,0x%02X,0x%02X,0x%02X, 0x%02X,0x%02X,0x%02X,0x%02X,",
					buf[i],
					buf[i+1],
					buf[i+2],
					buf[i+3],
					buf[i+4],
					buf[i+5],
					buf[i+6],
					buf[i+7],
					buf[i+8],
					buf[i+9],
					buf[i+10],
					buf[i+11],
					buf[i+12],
					buf[i+13],
					buf[i+14],
					buf[i+15]
				);
				std::cout << b << std::endl;
			}
		}
	}
	return 0;
}
