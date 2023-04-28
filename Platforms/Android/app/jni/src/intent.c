//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ intent interface for Android ]
//

#ifdef SDL

#ifdef __ANDROID__

#include <string.h>
#include <jni.h>
#include <android/log.h>
#include "xm8jni.h"

//
// intent buffer
//
static char intent_buffer[4096 * 3];

//
// nativeIntent()
// pass intent data to application through JNI
//
JNIEXPORT void JNICALL Java_net_retropc_pi_XM8_nativeIntent(JNIEnv* env, jclass jcls, jstring file_name)
{
	const char *src;
	unsigned char *dest;
	unsigned char high;
	unsigned char low;

	// get string
	const char *path = (*env)->GetStringUTFChars(env, file_name, NULL);
	__android_log_print(ANDROID_LOG_INFO, "XM8" ,"nativeIntent() path=""\x22""%s""\x22", path);

	// pass string to application
	src = path;
	dest = (unsigned char*)intent_buffer;
	while (*src != '\0') {
		// Java encodes UTF-8 string into '%hex' style
		if (*src == '%') {
			// get high and low
			high = (unsigned char)src[1];
			if (high == '\0') {
				break;
			}
			low = (unsigned char)src[2];
			if (low == '\0') {
				break;
			}
			src += 3;

			// high
			if ((high >= '0') && (high <= '9')) {
				high -= '0';
			}
			else {
				high |= 0x20;
				high -= 0x57;
			}
			high <<= 4;

			// low
			if ((low >= '0') && (low <= '9')) {
				low -= '0';
			}
			else {
				low |= 0x20;
				low -= 0x57;
			}

			*dest++ = (unsigned char)(high | low);
		}
		else {
			*dest++ = (unsigned char)*src++;
		}
	}

	// terminate string
	*dest = '\0';

	// release string
	(*env)->ReleaseStringUTFChars(env, file_name, path);
}

//
// Android_HasIntent()
// check valid/invalid intent
//
int Android_HasIntent(void)
{
	// check top character
	if (intent_buffer[0] != '\0') {
		return 1;
	}

	return 0;
}

//
// Android_GetIntent()
// get intent buffer
//
const char* Android_GetIntent(void)
{
	return intent_buffer;
}

//
// Android_ClearIntent()
// clear intent buffer
//
void Android_ClearIntent(void)
{
	intent_buffer[0] = '\0';
}

#endif // __ANDROID__

#endif // SDL
