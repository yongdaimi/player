#include <jni.h>
#include <string>

#include <android/log.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>



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


// 顶点着色器
#define GET_STR(x) #x
static const char *vertexShader = GET_STR(
        attribute vec4 aPosition; //顶点坐标
        attribute vec2 aTexCoord; //材质顶点坐标
        varying vec2 vTexCoord;   //输出的材质坐标
        void main(){
            vTexCoord = vec2(aTexCoord.x,1.0-aTexCoord.y);
            gl_Position = aPosition;
        }
);

//片元着色器,软解码和部分x86硬解码
static const char *fragYUV420P = GET_STR(
        precision mediump float;    //精度
        varying vec2 vTexCoord;     //顶点着色器传递的坐标
        uniform sampler2D yTexture; //输入的材质（不透明灰度，单像素）
        uniform sampler2D uTexture;
        uniform sampler2D vTexture;
        void main(){
            vec3 yuv;
            vec3 rgb;
            yuv.r = texture2D(yTexture,vTexCoord).r;
            yuv.g = texture2D(uTexture,vTexCoord).r - 0.5;
            yuv.b = texture2D(vTexture,vTexCoord).r - 0.5;
            rgb = mat3(1.0,     1.0,    1.0,
                       0.0,-0.39465,2.03211,
                       1.13983,-0.58060,0.0)*yuv;
            //输出像素颜色
            gl_FragColor = vec4(rgb,1.0);
        }
);

GLint InitShader(const char *code, GLint type)
{
    // 创建Shader
    GLint sh = glCreateShader(type);
    if (sh == 0) {
        LOGE("glCreateShader failed, error type is: %d", type);
        return 0;
    }
    // 加载Shader
    glShaderSource(
            sh,
            1,              // shader数量
            &code,          // shader代码
            0               // 代码长度
            );
    // 编译Shader
    glCompileShader(sh);
    // 获取编译结果
    GLint status;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        LOGE("glCompileShader failed");
        return 0;
    }
    LOGI("glCompileShader success");
    return sh;
}




extern "C" JNIEXPORT jstring
JNICALL
Java_com_yuneec_testopengles_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}


extern "C"
JNIEXPORT void JNICALL
Java_com_yuneec_testopengles_Xplay_open(JNIEnv *env, jobject instance, jstring url_,
                                        jobject surface) {
    const char *url = env->GetStringUTFChars(url_, 0);

    // 获取原始窗口
    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);
    // EGL
    // 1. EGL display 的创建和初始化
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay() failed");
        return;
    }
    if (EGL_TRUE != eglInitialize(display, 0, 0)) {
        LOGE("eglInitialize() failed");
        return;
    }
    // 2. Surface
    // surface 窗口配置
    // 输出配置
    EGLConfig config;
    EGLint configNum;
    EGLint configSpec[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE
    };
    if (EGL_TRUE != eglChooseConfig(display, configSpec, &config, 1, &configNum)) {
        LOGE("eglChooseConfig() failed");
        return;
    }
    // 创建Surface
    EGLSurface winSurface = eglCreateWindowSurface(display, config, nwin, 0);
    if (winSurface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface() failed");
        return;
    }

    //3. context 创建关联的上下文
    const EGLint ctxAttr[] = {
            EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttr);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed");
        return;
    }
    if (EGL_TRUE != eglMakeCurrent(display, winSurface, winSurface, context)) {
        LOGE("eglMakeCurrent failed");
        return;
    }
    LOGI("EGL init success");

    // 顶点Shader初始化
    GLint vsh = InitShader(vertexShader, GL_VERTEX_SHADER);
    // 片元YUV420p shader初始化
    GLint fsh = InitShader(fragYUV420P, GL_FRAGMENT_SHADER);



    env->ReleaseStringUTFChars(url_, url);
}