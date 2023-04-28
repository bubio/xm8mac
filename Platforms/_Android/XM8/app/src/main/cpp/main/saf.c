//
// eXcellent Multi-platform emulator type 8 - 'XM8'
// based on ePC-8801MA
//
// Author (ePC-8801MA) : Takeda.Toshiya
// Author (XM8) : Tanaka.Yasushi
//
// [ storage access interface for Android ]
//

#ifdef SDL

#ifdef __ANDROID__

#include <string.h>
#include <jni.h>
#include <pthread.h>
#include <android/log.h>
#include "os.h"
#include "xm8jni.h"

//
// log tag
//
#define LOG_TAG						"XM8"
										// for __android_log_print() function

//
// path data (see emu_sdl.cpp)
//
#define EXTERNAL_PATH_ANDROID		"/Android/data/"
										// SDL_AndroidGetExtrernalStoragePath()


//
// Java class and method
//
#define JAVA_CLASS_NAME				"net/retropc/pi/XM8"
										// class name
#define JAVA_REQUEST_METHOD_NAME	"requestActivity"
										// method name (requestActivity)
#define JAVA_REQUEST_SIGNATURE_NAME	"()V"
										// sigunature name (requestActivity)
#define JAVA_GET_METHOD_NAME		"getFileDescriptor"
										// method name (getFileDescriptor)
#define JAVA_GET_SIGNATURE_NAME		"(Ljava/lang/String;I)I"
										// signature name (getFileDescriptor)
#define JAVA_CLEAR_METHOD_NAME		"clearTreeUri"
										// method name (clearTreeUri)
#define JAVA_CLEAR_SIGNATURE_NAME	"()V"
										// signature name (clearTreeUri)

//
// static variable
//
static pthread_key_t java_thread_key;
										// pthread key for detach
static JavaVM *java_vm;
										// Java virtual machine
static int java_attached;
										// Java AttachCurrentThread flag
static jclass java_class;
										// Java activity class
static int sdk_version;
										// Build.VERSION.SDK_INT
static int skip_main;
										// skip flag (main)
static char abs_dir[0x200];
										// Activity.GetExternalFilesDir(null).GetAbsolutePath()
static char ext_dir[0x200];
										// Activity.GetExternalFilesDirs(null) if removable
static char tree_uri[0x200];
										// Uri got by Intent.ACTION_OPEN_DOCUMENT_TREE

//
// in SDL library
//
extern void Android_JNI_PollInputDevices(void);
										// SDL_android.c
extern void SDL_SYS_JoystickDetect(void);
										// SDL_joystick.c
extern SDL_sem *Android_ResumeSem;
										// SDL_androidevents.c

//
// JNI_DetachThread()
// detach thread 
//
static void JNI_DetachThread(void *value)
{
	JNIEnv *env;

	// get Java environment
	env = (JNIEnv*)value;
	if (env != NULL) {
		if (java_attached != 0) {
			// detach thread
			__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "JNI:DetachCurrentThread");
			(*java_vm)->DetachCurrentThread(java_vm);

			// detached
			java_attached = 0;
		}

		// clear pthread key
		pthread_setspecific(java_thread_key, NULL);
	}
}

//
// JNI_OnLoad()
// hook function on loading libmain.so
//
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	// not attached
	java_attached = 0;

	// save java vm
	java_vm = vm;

	// create pthread_key for detach thread
	pthread_key_create(&java_thread_key, JNI_DetachThread);

	// see SDL_android.c
	return JNI_VERSION_1_4;
}

//
// JNI_GetEnvironment()
// get Java Environment for each thread
//
static JNIEnv* JNI_GetEnvironment(void)
{
	JNIEnv *env;
	int status;

	// attach current thread
	status = (*java_vm)->AttachCurrentThread(java_vm, &env, NULL);
	if (status < 0) {
		return NULL;
	}

	// attached
	if (java_attached == 0) {
		__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "JNI:AttachCurrentThread");
	}
	java_attached = 1;
	pthread_setspecific(java_thread_key, (void*)env);

	return env;
}

// nativeBuildVer()
// set Build.VERSION.SDK_INT
//
JNIEXPORT void JNICALL Java_net_retropc_pi_XM8_nativeBuildVer(JNIEnv *env, jclass jcls, jint ver)
{
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Build.VERSION.SDK_INT=%d", (int)ver);

	// save activity class
	java_class = (jclass)((*env)->NewGlobalRef(env, jcls));

	// set running sdk version
	sdk_version = ver;
}

//
// nativeAbsDir()
// set Activity.GetExternalFilesDir(null).GetAbsolutePath()
//
JNIEXPORT void JNICALL Java_net_retropc_pi_XM8_nativeAbsDir(JNIEnv *env, jclass jcls, jstring jabs_dir)
{
	int len;
	char *root;

	// get string
	const char *dir = (*env)->GetStringUTFChars(env, jabs_dir, NULL);

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "AbsoluteDir=%s", dir);

	// copy
	len = strlen(dir);
	if (len < sizeof(abs_dir)) {
		strcpy(abs_dir, dir);

		root = strstr(abs_dir, EXTERNAL_PATH_ANDROID);
		if (root != NULL) {
			// like "/storage/emulated/0/"
			root[1] = '\0';
		}
	}

	// release string
	(*env)->ReleaseStringUTFChars(env, jabs_dir, dir);
}

//
// nativeExtDir()
// set Activity.GetExternalFilesDirs(null) if removable
//
JNIEXPORT void JNICALL Java_net_retropc_pi_XM8_nativeExtDir(JNIEnv *env, jclass jcls, jstring jext_dir)
{
	int len;

	// get string
	const char *dir = (*env)->GetStringUTFChars(env, jext_dir, NULL);

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "ExternalDir=%s", dir);

	// copy
	len = strlen(dir);
	if (len < (sizeof(ext_dir) - 1)) {
		strcpy(ext_dir, dir);

		if (ext_dir[len - 1] != '/') {
			strcat(ext_dir, "/");
		}
	}

	// release string
	(*env)->ReleaseStringUTFChars(env, jext_dir, dir);
}

//
// nativeUri()
// set tree Uri for SD card
//
JNIEXPORT void JNICALL Java_net_retropc_pi_XM8_nativeUri(JNIEnv *env, jclass jcls, jstring jtree_uri)
{
	int len;

	// get string
	const char *uri = (*env)->GetStringUTFChars(env, jtree_uri, NULL);

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "treeUri=%s", uri);

	// copy
	len = strlen(uri);
	if (len < sizeof(tree_uri)) {
		strcpy(tree_uri, uri);
	}

	// release string
	(*env)->ReleaseStringUTFChars(env, jtree_uri, uri);
}

//
// nativeSkipMain()
// set skip flag (main)
//
JNIEXPORT void JNICALL Java_net_retropc_pi_XM8_nativeSkipMain(JNIEnv *env, jclass jcls, jint skip)
{
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "SkipMain=%d", (int)skip);
	skip_main = skip;
}

//
// nativeDelete()
// call DeleteGlobalRef()
//
JNIEXPORT void JNICALL Java_net_retropc_pi_XM8_nativeDelete(JNIEnv *env, jclass jcls)
{
	// save activity class
	if (java_class != NULL) {
		(*env)->DeleteGlobalRef(env, java_class);
		java_class = NULL;
	}
}

//
// Android_RequestActivity()
// request activity for SAF
//
void Android_RequestActivity(void)
{
	JNIEnv *env;
	jclass jcls;
	jmethodID id;

	// get Java environment
	env = JNI_GetEnvironment();
	if (env == NULL) {
		return;
	}

	// find class
	jcls = (*env)->FindClass(env, JAVA_CLASS_NAME);
	if (jcls == NULL) {
		return;
	}

	// get method id
	id = (*env)->GetMethodID(env, jcls, JAVA_REQUEST_METHOD_NAME, JAVA_REQUEST_SIGNATURE_NAME);
	if (id == NULL) {
		return;
	}

	// call
	(*env)->CallVoidMethod(env, java_class, id);
}

//
// Android_HasExternalSD()
// get external SD flag
//
int Android_HasExternalSD(void)
{
	if (ext_dir[0] != '\0') {
		return 1;
	}
	else {
		return 0;
	}
}

//
// Android_HasTreeUri()
// get treeUri flag
//
int Android_HasTreeUri(void)
{
	if (tree_uri[0] != '\0') {
		return 1;
	}
	else {
		return 0;
	}
}

//
// Android_ClearTreeUri()
// clear treeUri
//
void Android_ClearTreeUri(void)
{
	JNIEnv *env;
	jclass jcls;
	jmethodID id;

	// clear
	tree_uri[0] = '\0';

	// get Java environment
	env = JNI_GetEnvironment();
	if (env == NULL) {
		return;
	}

	// find class
	jcls = (*env)->FindClass(env, JAVA_CLASS_NAME);
	if (jcls == NULL) {
		return;
	}

	// get method id
	id = (*env)->GetMethodID(env, jcls, JAVA_CLEAR_METHOD_NAME, JAVA_CLEAR_SIGNATURE_NAME);
	if (id == NULL) {
		return;
	}

	// call
	(*env)->CallVoidMethod(env, java_class, id);
}

//
// Android_IsExternalSD()
// check path for external SD
//
int Android_IsExternalSD(const char *path)
{
	int len;

	if (Android_HasExternalSD() == 0) {
		return 0;
	}

	// vaild ext_dir[]
	len = strlen(ext_dir);
	if (strncmp(path, ext_dir, len) == 0) {
		// external SD
		return 1;
	}

	// not external SD
	return 0;
}

//
// Android_GetFileDescriptor()
// get file descriptor through storage access framework (for Android 5.0 or later)
//
int Android_GetFileDescriptor(const char *path, int type)
{
	JNIEnv *env;
	jclass jcls;
	jmethodID id;
	jstring jstr;

	// get Java environment
	env = JNI_GetEnvironment();
	if (env == NULL) {
		return -1;
	}

	// find class
	jcls = (*env)->FindClass(env, JAVA_CLASS_NAME);
	if (jcls == NULL) {
		return -1;
	}

	// get method id
	id = (*env)->GetMethodID(env, jcls, JAVA_GET_METHOD_NAME, JAVA_GET_SIGNATURE_NAME);
	if (id == NULL) {
		return -1;
	}

	// call
	jstr = (*env)->NewStringUTF(env, path);
	return (*env)->CallIntMethod(env, java_class, id, jstr, type);
}

//
// Android_ChDir()
// change directory (internal storage <-> exernal storage)
//
int Android_ChDir(char *dir, const char *name)
{
	// check SDK version
	if (Android_GetSdkVersion() < 21) {
		return 0;
	}

	// "../" only
	if (strcmp(name, "../") != 0) {
		return 0;
	}

	// compare
	if (strcmp(abs_dir, dir) == 0) {
		// go to external storage or keep current directory
		if ((Android_HasExternalSD() != 0) && (Android_HasTreeUri() != 0)) {
			// go to external storage
			strcpy(dir, ext_dir);
			return 1;
		}

		// keep current directory
		strcpy(dir, abs_dir);
		return 1;
	}

	if (Android_HasExternalSD() != 0) {
		// compare
		if (strcmp(ext_dir, dir) == 0) {
			// go to internal storage
			strcpy(dir, abs_dir);
			return 1;
		}
	}

	return 0;
}

//
// Android_GetSdkVersion()
// get Build.VERSION.SDK_INT
//
int Android_GetSdkVersion(void)
{
	return sdk_version;
}

//
// Android_ChkSkipMain()
// check skip flag (main)
//
int Android_ChkSkipMain(void)
{
	return skip_main;
}

//
// Android_PollJoystick()
// call SDLControllerManager.pollInputDevices()
//
void Android_PollJoystick(void)
{
	// restore pause/resume behavior (force resume)
	if (!SDL_SemValue(Android_ResumeSem)) {
		SDL_SemPost(Android_ResumeSem);
	}

	// see SDL_SYS_JoystickDetect() in SDL_sysjoystick.c (static variables are not initialized after second launch)
	Android_JNI_PollInputDevices();
	SDL_SYS_JoystickDetect();
}

#endif // __ANDROID__

#endif // SDL
