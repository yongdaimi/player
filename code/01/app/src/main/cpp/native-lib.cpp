#include <jni.h>
#include <string>
#include <android/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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

/**避免分母为0*/
static double r2d(AVRational r) {
    return r.num == 0 || r.den == 0 ? 0 : (double)r.num / (double)r.den;
}


extern "C" JNIEXPORT jstring
JNICALL
Java_com_yuneec_yongdaimi_ff_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    hello += avcodec_configuration();

    // 初始化解封装
    av_register_all();
    // 注册解码器
    avcodec_register_all();

    // 初始化网络
    avformat_network_init();
    // 打开文件
    AVFormatContext *ic = NULL;
    char path[] = "sdcard/1080.mp4";
    // char path[] = "/sdcard/qingfeng.flv";
    int ret = avformat_open_input(&ic, path, 0, 0);
    if (ret != 0) {
        LOGE("avformat_open_input() called failed: %s", av_err2str(ret));
        return env->NewStringUTF(hello.c_str());
    }
    LOGI("avformat_open_input() called success.");
    LOGI("duration is: %lld, nb_stream is: %d", ic->duration, ic->nb_streams);
    if (avformat_find_stream_info(ic, 0) >=0 ) {
        LOGI("duration is: %lld, nb_stream is: %d", ic->duration, ic->nb_streams);
    }

    /**帧率*/
    int fps = 0;
    int videoStream = 0;
    int audioStream = 1;

    for (int i = 0; i < ic->nb_streams; i++) {
        AVStream *as = ic->streams[i];
        if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            LOGI("视频数据");
            videoStream = i;
            fps = (int)r2d(as->avg_frame_rate);
            LOGI("fps = %d, width = %d, height = %d, codecid = %d, format = %d",
                 fps,
                 as->codecpar->width,
                 as->codecpar->height,
                 as->codecpar->codec_id,
                 as->codecpar->format);
        } else if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            LOGI("音频数据");
            audioStream = i;
            LOGI("sample_rate = %d, channels = %d, sample_format = %d",
                 as->codecpar->sample_rate,
                 as->codecpar->channels,
                 as->codecpar->format
            );
        }
    }

    AVCodec *vc = avcodec_find_decoder(ic->streams[videoStream]->codecpar->codec_id); // 软解
    // vc = avcodec_find_decoder_by_name("h264_mediacodec"); // 硬解
    if (!vc) {
        LOGE("avcodec_find_decoder[videoStream] failure");
        return env->NewStringUTF(hello.c_str());
    }
    // 配置解码器
    AVCodecContext *vct = avcodec_alloc_context3(vc);
    avcodec_parameters_to_context(vct, ic->streams[videoStream]->codecpar);
    vct->thread_count = 1;
    // 打开解码器
    int re = avcodec_open2(vct, vc, 0);
    if (re != 0) {
        LOGE("avcodec_open2 failure");
        return env->NewStringUTF(hello.c_str());
    }


    /*
    // 读取帧数据
    AVPacket *packet = av_packet_alloc();
    for (;;) {
        int ret = av_read_frame(ic, packet);
        if (ret != 0) {
            LOGI("读取到结尾处");
            int pos = 20 * r2d(ic->streams[videoStream]->time_base);
            // 改变播放进度
            av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
            continue;
        }
        LOGI("streamIndex=%d, size=%d, pts=%lld, flag=%d",
             packet->stream_index,
             packet->size,
             packet->pts,
             packet->flags
        );
        av_packet_unref(packet);
    }
     */

    // audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    // LOGI("av_find_best_stream, audio index is: %d", audioStream);

    // 关闭上下文
    avformat_close_input(&ic);
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


