#include <jni.h>
#include <string>
#include <android/log.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/jni.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
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
    return env->NewStringUTF(hello.c_str());
}


extern "C"
JNIEXPORT
jint JNI_OnLoad(JavaVM *vm,void *res)
{
    av_jni_set_java_vm(vm,0);
    return JNI_VERSION_1_4;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_yuneec_yongdaimi_ff_XPlay_open(JNIEnv *env, jobject instance, jstring url_,
                                        jobject surface) {
    const char *path = env->GetStringUTFChars(url_, 0);

    // 初始化解封装
    av_register_all();
    // 注册解码器
    avcodec_register_all();

    // 初始化网络
    avformat_network_init();
    // 打开文件
    AVFormatContext *ic = NULL;
    // char path[] = "/sdcard/qingfeng.flv";
    int ret = avformat_open_input(&ic, path, 0, 0);
    if (ret != 0) {
        LOGE("avformat_open_input() called failed: %s", av_err2str(ret));
        return;
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
    vCodec = avcodec_find_decoder_by_name("h264_mediacodec"); // 硬解
    if (!vCodec) {
        LOGE("avcodec_find_decoder() failed. can not found video decoder.");
        return ;
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
        return;
    }

    // 查找音频解码器
    AVCodec *aCodec = avcodec_find_decoder(ic->streams[audioStream]->codecpar->codec_id); // 软解
    // aCodec= avcodec_find_decoder_by_name("h264_mediacodec"); // 硬解
    if (!aCodec) {
        LOGE("avcodec_find_decoder() failed. can not found audio decoder.");
        return;
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
        return ;
    }

    // 读取帧数据
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int64_t start = getNowMs();
    int frameCount = 0;

    // 初始化像素格式转换上下文
    SwsContext *vctx = NULL;
    int outWidth = 1280;
    int outHeight = 720;
    char *rgb = new char[1920*1080*4];
    char *pcm = new char[48000*4*2];

    // 初始化音频重采样上下文
    SwrContext *actx = swr_alloc();
    actx = swr_alloc_set_opts(
            actx,
            av_get_default_channel_layout(2),
            AV_SAMPLE_FMT_S16,
            ac->sample_rate,
            av_get_default_channel_layout(ac->channels),
            ac->sample_fmt,
            ac->sample_rate,
            0, 0
    );

    ret = swr_init(actx);
    if (ret != 0) {
        LOGE("swr_init failed");
    } else {
        LOGI("swr_init success");
    }

    // 显示窗口初始化
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("ANativeWindow_fromSurface create failed");
        return;
    }
    ANativeWindow_setBuffersGeometry(window, outWidth, outHeight, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer wbuf;


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
                vctx = sws_getCachedContext(
                        vctx,
                        frame->width,
                        frame->height,
                        (AVPixelFormat)frame->format,
                        outWidth,
                        outHeight,
                        AV_PIX_FMT_RGBA,
                        SWS_FAST_BILINEAR,
                        0, 0, 0
                );
                if (!vctx) {
                    LOGE("sws_getCachedContext failed!");
                } else {
                    // 开始像素格式转换
                    uint8_t  *data[AV_NUM_DATA_POINTERS] = {0};
                    data[0] = (uint8_t *)rgb;
                    int lines[AV_NUM_DATA_POINTERS] = {0};
                    lines[0] = outWidth * 4;
                    int h = sws_scale(
                            vctx,
                            (const uint8_t **)frame->data,
                            frame->linesize,
                            0,
                            frame->height,
                            data, lines
                    );
                    LOGI("sws_scale = %d", h);
                    if (h > 0) {
                        ANativeWindow_lock(window, &wbuf, 0);
                        uint8_t *dst = (uint8_t *)wbuf.bits; // 这个dst是用来交换的内存
                        memcpy(dst, rgb, outWidth*outHeight*4);
                        ANativeWindow_unlockAndPost(window);
                    }
                }
            } else { // 音频部分
                uint8_t *out[2] = {0};
                out[0] = (uint8_t *)pcm;
                // 音频重采样
                int len = swr_convert(
                        actx,
                        out,
                        frame->nb_samples,
                        (const uint8_t **)frame->data,
                        frame->nb_samples
                );
                LOGI("swr_convert = %d", len);
            }
            // LOGI("Receive a frame.........");
        }
    }

    delete rgb;
    delete pcm;

    // 关闭上下文
    avformat_close_input(&ic);

    env->ReleaseStringUTFChars(url_, path);
}