//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ unicode converter ]
//

#ifdef SDL

#ifndef CONVERTER_H
#define CONVERTER_H

//
// unicode converter
//
class Converter
{
public:
	Converter();
										// constructor
	virtual ~Converter();
										// destructor
	bool Init();
										// initialize
	void Deinit();
										// deinitialize

	// convert
	void SjisToUtf(const char *sjis, char *utf);
										// convert shift-jis to UTF-8
	void UtfToSjis(const char *utf, char *sjis);
										// convert UTF-8 to shift-jis

private:
	Uint16 *ucs_table;
										// UCS-2 to shift-jis table
	static const Uint16 sjis_table[0x30 * 0xc0];
										// shift-jis to UCS-2 table
};

#endif // CONVERTER_H

#endif // SDL
