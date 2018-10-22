package com.yuneec.yongdaimi.ff;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.view.SurfaceHolder;

public class XPlay extends GLSurfaceView implements Runnable, SurfaceHolder.Callback{



    public static final String TEST_URL = "sdcard/1080.mp4";


    public XPlay(Context context) {
        super(context);
    }

    public XPlay(Context context, AttributeSet attrs) {
        super(context, attrs);
    }


    @Override
    public void run() {
        open(TEST_URL, getHolder().getSurface());
    }

    @Override
    public void surfaceCreated(SurfaceHolder var1) {
        new Thread(this).start();
    }

    @Override
    public void surfaceChanged(SurfaceHolder var1, int var2, int var3, int var4) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder var1) {

    }



    public native void open(String url, Object surface);



}



