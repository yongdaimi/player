#include <jni.h>
#include <string>
#include <android/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/jni.h>
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

/**
 * 避免分母为0
 * @param r AVRational 它是一个分数 包含分子和分母
 * @return  分子分母相除后的浮点数
 */
static double r2d(AVRational r) {
    return r.num == 0 || r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

/**
 * 获得当前毫秒数
 * @return 前毫秒数
 */
static int64_t getNowMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int sec = tv.tv_sec%360000;
    int64_t t = sec*1000+tv.tv_usec/1000;
    return t;
}


extern "C"
JNIEXPORT jstring
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
    LOGI("avformat_open_input(): File open success.");
    LOGI("File duration is: %lld, nb_stream is: %d", ic->duration, ic->nb_streams);
    if (avformat_find_stream_info(ic, 0) >=0 ) {
        LOGI("File duration is: %lld, nb_stream is: %d", ic->duration, ic->nb_streams);
    }

    /**帧率*/
    int fps = 0;
    /*视频流索引*/
    int videoStream = 0;
    /*音频流索引*/
    int audioStream = 1;

    // 遍历获得音/视频流索引
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

    // 也可以利用av_find_best_stream()函数来查找音视频流索引
    // audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    // LOGI("av_find_best_stream, audio index is: %d", audioStream);


    // 查找视频解码器
    AVCodec *vCodec = avcodec_find_decoder(ic->streams[videoStream]->codecpar->codec_id); // 软解
    // vCodec = avcodec_find_decoder_by_name("h264_mediacodec"); // 硬解
    if (!vCodec) {
        LOGE("avcodec_find_decoder() failed. can not found video decoder.");
        return env->NewStringUTF(hello.c_str());
    }
    // 配置解码器上下文
    AVCodecContext *vc = avcodec_alloc_context3(vCodec);
    // 将AVStream里面的参数复制到上下文当中
    avcodec_parameters_to_context(vc, ic->streams[videoStream]->codecpar);
    vc->thread_count = 8;
    // 打开解码器
    ret = avcodec_open2(vc, vCodec, 0);
    if (ret != 0) {
        LOGE("avcodec_open2() failed. can not open video decoder, line is: %d", __LINE__);
        return env->NewStringUTF(hello.c_str());
    }

    // 查找音频解码器
    AVCodec *aCodec = avcodec_find_decoder(ic->streams[audioStream]->codecpar->codec_id); // 软解
    // aCodec= avcodec_find_decoder_by_name("h264_mediacodec"); // 硬解
    if (!aCodec) {
        LOGE("avcodec_find_decoder() failed. can not found audio decoder.");
        return env->NewStringUTF(hello.c_str());
    }
    // 配置解码器上下文
    AVCodecContext *ac = avcodec_alloc_context3(aCodec);
    // 将AVStream里面的参数复制到上下文当中
    avcodec_parameters_to_context(ac, ic->streams[audioStream]->codecpar);
    ac->thread_count = 8;
    // 打开解码器
    ret = avcodec_open2(ac, aCodec, 0);
    if (ret != 0) {
        LOGE("avcodec_open2() failed. can not open audio decoder");
        return env->NewStringUTF(hello.c_str());
    }

    // 读取帧数据
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int64_t start = getNowMs();
    int frameCount = 0;

    for (;;) {

        if (getNowMs() - start >= 3000) {
            LOGI("now decoder fps is: %d", frameCount / 3);
            start = getNowMs();
            frameCount = 0;
        }
        int ret = av_read_frame(ic, packet);
        if (ret != 0) {
            LOGE("读取到结尾处");
            int pos = 20 * r2d(ic->streams[videoStream]->time_base);
            // 改变播放进度
            av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
            continue;
        }

//        LOGI("Read a Packet. streamIndex=%d, size=%d, pts=%lld, flag=%d",
//             packet->stream_index,
//             packet->size,
//             packet->pts,
//             packet->flags
//        );

        AVCodecContext *cc = vc;
        if (packet->stream_index == audioStream) cc = ac;
        // 发送到线程中去解码(将packet写入到解码队列当中去)
        ret = avcodec_send_packet(cc, packet);
        // 清理
        int p = packet->pts;
        av_packet_unref(packet);
        if (ret != 0) {
            // LOGE("avcodec_send_packet failed.");
            continue;
        }

        for(;;) {
            // 从已解码成功的数据当中取出一个frame, 要注意send一次,receive不一定是一次
            ret = avcodec_receive_frame(cc, frame);
            if (ret != 0) {
                break;
            }
            if (cc == vc) {
                frameCount++;
            }
            // LOGI("Receive a frame.........");
        }
    }

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


extern "C"
JNIEXPORT
jint JNI_OnLoad(JavaVM *vm,void *res)
{
    av_jni_set_java_vm(vm,0);
    return JNI_VERSION_1_4;
}








