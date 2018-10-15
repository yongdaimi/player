#include <jni.h>
#include <string>
#include <android/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#define LOG_TAG "xp.chen"
#ifdef LOG_TAG
    #define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
    #define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
    #define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE,LOG_TAG,__VA_ARGS__)
#else
    #define LOGI(...)
    #define LOGE(...)
    #define LOGV(...)
#endif

extern "C" JNIEXPORT jstring
JNICALL
Java_com_yuneec_yongdaimi_ff_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    hello += avcodec_configuration();
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_yuneec_yongdaimi_ff_MainActivity_open(JNIEnv *env, jobject instance, jstring uri_,
                                               jobject handle) {
    const char *uri = env->GetStringUTFChars(uri_, 0);
    FILE *fp = fopen(uri, "rb");
    if (fp) {
        LOGI("file open success");
    } else {
        LOGI("file open error");
    }
    env->ReleaseStringUTFChars(uri_, uri);
    return JNI_TRUE;
}


