#include <jni.h>
#include <string>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>


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


static SLObjectItf engineSL = NULL;
SLEngineItf CreateSL()
{
    SLresult ret;
    SLEngineItf en;
    // 创建引擎
    ret = slCreateEngine(&engineSL, 0, 0, 0, 0, 0);
    if (ret != SL_RESULT_SUCCESS) {
        LOGE("slCreateEngine() failed ");
        return NULL;
    }
    // 实例化
    ret = (*engineSL)->Realize(engineSL, SL_BOOLEAN_FALSE);
    if (ret != SL_RESULT_SUCCESS) {
        LOGE("Realize failed");
        return NULL;
    }
    // 获取接口
    ret = (*engineSL)->GetInterface(engineSL, SL_IID_ENGINE, &en);
    if (ret != SL_RESULT_SUCCESS) {
        LOGE("GetInterface failed");
        return NULL;
    }
    return en;
}

void PcmCall(SLAndroidSimpleBufferQueueItf bf,void *contex)
{
    LOGI("PcmCall");
    static FILE *fp = NULL;
    static char *buf = NULL;
    if(!buf)
    {
        buf = new char[1024*1024];
    }
    if(!fp)
    {
        fp = fopen("/sdcard/test.pcm","rb");
    }
    if(!fp)return;
    if(feof(fp) == 0)
    {
        int len = fread(buf,1,1024,fp);
        if(len > 0)
            (*bf)->Enqueue(bf,buf,len);
    }
}


extern "C" JNIEXPORT jstring
JNICALL
Java_com_yuneec_yongdaimi_testopensl_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    // 1. 创建引擎
    SLEngineItf eng = CreateSL();
    if (eng) {
        LOGI("CreateSL success");
    } else {
        LOGE("CreateSL failed");
    }
    // 2. 创建混音器
    SLObjectItf mix = NULL;
    SLresult ret = 0;
    ret = (*eng)->CreateOutputMix(eng, &mix, 0, 0, 0);
    if (ret != SL_RESULT_SUCCESS) {
        LOGE("CreateOutputMix failed");
    }
    ret = (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    if (ret != SL_BOOLEAN_FALSE) {
        LOGE("(*mix)->Realize failed!");
    }
    SLDataLocator_OutputMix outmix = {SL_DATALOCATOR_OUTPUTMIX, mix};
    SLDataSink audioSink = {&outmix, 0};
    // 3. 配置音频信息
    // 缓冲队列
    SLDataLocator_AndroidSimpleBufferQueue que = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 10};
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,
            2, // 声道数
            SL_SAMPLINGRATE_44_1,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN //字节序，小端
    };
    SLDataSource ds = {&que, &pcm};

    // 创建播放器
    SLObjectItf player = NULL;
    SLPlayItf iplayer = NULL;
    SLAndroidSimpleBufferQueueItf pcmQue = NULL;
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};
    ret = (*eng)->CreateAudioPlayer(eng, &player, &ds, &audioSink, sizeof(ids)/sizeof(SLInterfaceID), ids, req);
    if (ret != SL_RESULT_SUCCESS) {
        LOGE("CreateAudioPlayer failed");
    } else {
        LOGI("CreateAudioPlayer success");
    }


    (*player)->Realize(player,SL_BOOLEAN_FALSE);
    //获取player接口
    ret = (*player)->GetInterface(player,SL_IID_PLAY,&iplayer);
    if(ret !=SL_RESULT_SUCCESS )
    {
        LOGE("GetInterface SL_IID_PLAY failed!");
    }
    ret = (*player)->GetInterface(player,SL_IID_BUFFERQUEUE,&pcmQue);
    if(ret !=SL_RESULT_SUCCESS )
    {
        LOGE("GetInterface SL_IID_BUFFERQUEUE failed!");
    }

    //设置回调函数，播放队列空调用
    (*pcmQue)->RegisterCallback(pcmQue,PcmCall,0);

    //设置为播放状态
    (*iplayer)->SetPlayState(iplayer,SL_PLAYSTATE_PLAYING);

    //启动队列回调
    (*pcmQue)->Enqueue(pcmQue,"",1);
    return env->NewStringUTF(hello.c_str());
}
