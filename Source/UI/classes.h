//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ class declaration ]
//

#ifdef SDL

#ifndef CLASSES_H
#define CLASSES_H

//
// SDL application
//
class App;

//
// SDL component
//
class Platform;
class Setting;
class Video;
class Audio;
class Font;
class Input;
class Menu;
class MenuList;
class MenuItem;
class DiskManager;
class TapeManager;
class Converter;
class SoftKey;

//
// win32-SDL i/f
//
class EMU_SDL;

//
// emulator
//
class EMU;
class VM;
class EVENT;
class PC88;
class UPD1990A;
class UPD765A;
class FILEIO;

#endif // CLASSES_H

#endif // SDL
