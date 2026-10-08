package dev.antigravity.microsipclone;

import android.app.Activity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.FrameLayout;

// Where the other side's picture is drawn during a video call: an Android
// surface laid over the part of the window the call screen leaves free for
// it. PJSIP renders into it with OpenGL; Qt only decides where it goes.
public class VideoSurface {
    private static SurfaceView view;

    // Implemented in CallService.cpp. Null when the surface is gone.
    private static native void surfaceChanged(Surface surface);

    // The rectangle is in screen pixels; an empty one takes the surface away
    public static void place(final Activity activity, final int x, final int y,
                             final int width, final int height) {
        activity.runOnUiThread(new Runnable() {
            public void run() {
                if (width <= 0 || height <= 0) {
                    if (view != null)
                        view.setVisibility(SurfaceView.GONE);
                    return;
                }
                FrameLayout.LayoutParams where = new FrameLayout.LayoutParams(width, height);
                where.leftMargin = x;
                where.topMargin = y;
                if (view == null) {
                    view = new SurfaceView(activity);
                    // Above Qt's own surface, which fills the window
                    view.setZOrderMediaOverlay(true);
                    view.getHolder().addCallback(new SurfaceHolder.Callback() {
                        public void surfaceCreated(SurfaceHolder holder) {}
                        public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
                            VideoSurface.surfaceChanged(holder.getSurface());
                        }
                        public void surfaceDestroyed(SurfaceHolder holder) {
                            VideoSurface.surfaceChanged(null);
                        }
                    });
                    activity.addContentView(view, where);
                } else {
                    view.setLayoutParams(where);
                }
                view.setVisibility(SurfaceView.VISIBLE);
            }
        });
    }
}
