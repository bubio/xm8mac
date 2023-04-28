//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ Java Native Interface for Android ]
//

#ifdef SDL

#ifdef __ANDROID__

#ifndef XM8JNI_H
#define XM8JNI_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//
// Android_HasIntent()
// check valid/invalid intent
//
int Android_HasIntent(void);

//
// Android_GetIntent()
// get intent buffer
//
const char* Android_GetIntent(void);

//
// Android_ClearIntent()
// clear intent buffer
//
void Android_ClearIntent(void);

//
// Android_GetSdkVersion()
// get Build.VERSION.SDK_INT
//
int Android_GetSdkVersion(void);

//
// Android_ChkSkipMain()
// check skip flag (main app)
//
int Android_ChkSkipMain(void);

//
// Android_RequestActivity()
// request start activity on next launch
//
void Android_RequestActivity(void);

//
// Android_HasExternalSD()
// get external SD flag
//
int Android_HasExternalSD(void);

//
// Android_HasTreeUri()
// get treeUri flag
//
int Android_HasTreeUri(void);

//
// Android_ClearTreeUri()
// clear treeUri
//
void Android_ClearTreeUri(void);

//
// Android_IsExternalSD()
// check path for external SD
//
int Android_IsExternalSD(const char *path);

//
// Android_GetFileDescriptor()
// get file descriptor through storage access framework (for Android 5.0 or later)
//
int Android_GetFileDescriptor(const char *path, int type);

//
// Android_ChDir()
// change directory (internal storage <-> exernal storage)
//
int Android_ChDir(char *dir, const char *name);

//
// Android_PollJoystick()
// call SDLControllerManager.pollInputDevices()
//
void Android_PollJoystick(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // XM8JNI_H

#endif // __ANDROID__

#endif // SDL
