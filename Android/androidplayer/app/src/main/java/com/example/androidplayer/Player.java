package com.example.androidplayer;

import android.view.Surface;

public class Player {
    private long nativeContext;

    public enum PlayerState {
        None,
        Playing,
        Paused,
        End,
        Seeking
    }

    private Surface mSurface;
    private PlayerState mState = PlayerState.None;
    private String fileUri;
    private double duration;

    public void setDataSource(String uri) {
        fileUri = uri;
    }

    public void setSurface(Surface surface) {
        mSurface = surface;
    }

    public void start() {
        nativePlay(fileUri, mSurface);
        mState = PlayerState.Playing;
        duration = nativeGetDuration();
    }

    public void pause(boolean p) {
        nativePause(p);
        if (p) {
            mState = PlayerState.Paused;
        } else {
            mState = PlayerState.Playing;
        }
    }

    public void stop() {
        nativeStop();
        mState = PlayerState.End;
    }

    public void seek(double position) {
        nativeSeek(position);
    }

    public double getProgress() {
        return nativeGetPosition() / duration;
    }

    public PlayerState getState() {
        return mState;
    }

    public void setSpeed(float speed) {
        nativeSetSpeed(speed);
    }

    // 调用解码并保存YUV的native方法
    public native void nativeRunDecode(String inputPath, String outputPath);

    // 封装Java调用接口，启动解码保存YUV流程
    public void startDecode(String inputPath, String outputPath) {
        nativeRunDecode(inputPath, outputPath);
    }

    private native int nativePlay(String file, Surface surface);
    private native void nativePause(boolean p);
    private native int nativeSeek(double position);
    private native int nativeStop();
    private native int nativeSetSpeed(float speed);
    private native double nativeGetPosition();
    private native double nativeGetDuration();

    static {
        System.loadLibrary("androidplayer");
    }
}
