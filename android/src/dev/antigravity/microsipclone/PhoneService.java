package dev.antigravity.microsipclone;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import java.util.ArrayList;

// Keeps the app alive and registered while it is not on screen, so that calls
// still arrive, and announces those calls. It runs in the app's own process:
// the SIP stack is the one the window uses, this only tells Android not to
// stop it.
//
// ponytail: holds the CPU and Wi-Fi awake for as long as the app runs, which
// costs battery, and a phone in deep sleep can still miss a call. Push
// notifications from the SIP provider are the way past both.
public class PhoneService extends Service {
    private static final String RUNNING = "running", CALLS = "calls";
    private static final int RUNNING_ID = 1, CALL_ID = 2;

    private PowerManager.WakeLock cpu;
    private WifiManager.WifiLock wifi;

    public static void start(Context context) {
        Intent service = new Intent(context, PhoneService.class);
        if (Build.VERSION.SDK_INT >= 26)
            context.startForegroundService(service);
        else
            context.startService(service);
    }

    // Rings through the notification, which Android shows over the lock
    // screen or as a banner; touching it opens the app on the call
    public static void incomingCall(Context context, String from) {
        manager(context).notify(CALL_ID, builder(context, CALLS)
            .setContentTitle("Incoming call")
            .setContentText(from)
            .setCategory(Notification.CATEGORY_CALL)
            .setPriority(Notification.PRIORITY_MAX)
            .setDefaults(Notification.DEFAULT_ALL)
            .setFullScreenIntent(openApp(context), true)
            .setAutoCancel(true)
            .build());
    }

    // Call audio. Android only routes it to the earpiece, a headset or the
    // loudspeaker while the phone is in "communication" mode.
    public static void inCall(Context context, boolean active) {
        AudioManager audio = (AudioManager) context.getSystemService(AUDIO_SERVICE);
        audio.setMode(active ? AudioManager.MODE_IN_COMMUNICATION : AudioManager.MODE_NORMAL);
        if (!active)
            setAudioOutput(context, "");
    }

    private static String outputName(AudioDeviceInfo device) {
        switch (device.getType()) {
        case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE: return "Earpiece";
        case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER: return "Loudspeaker";
        case AudioDeviceInfo.TYPE_WIRED_HEADSET:
        case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
        case AudioDeviceInfo.TYPE_USB_HEADSET: return "Headset";
        case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
        case 26 /* TYPE_BLE_HEADSET */: return "Bluetooth";
        default: return null;
        }
    }

    // What a call can be heard through right now, the one in use first
    public static String[] audioOutputs(Context context) {
        AudioManager audio = (AudioManager) context.getSystemService(AUDIO_SERVICE);
        ArrayList<String> names = new ArrayList<String>();
        if (Build.VERSION.SDK_INT >= 31) {
            AudioDeviceInfo current = audio.getCommunicationDevice();
            if (current != null && outputName(current) != null)
                names.add(outputName(current));
            for (AudioDeviceInfo device : audio.getAvailableCommunicationDevices()) {
                String name = outputName(device);
                if (name != null && !names.contains(name))
                    names.add(name);
            }
        } else {
            // ponytail: no Bluetooth choice before Android 12; it needs the
            // older SCO calls. A plugged-in headset replaces the earpiece.
            String quiet = audio.isWiredHeadsetOn() ? "Headset" : "Earpiece";
            names.add(audio.isSpeakerphoneOn() ? "Loudspeaker" : quiet);
            names.add(audio.isSpeakerphoneOn() ? quiet : "Loudspeaker");
        }
        return names.toArray(new String[0]);
    }

    // An empty name gives the choice back to Android
    public static void setAudioOutput(Context context, String name) {
        AudioManager audio = (AudioManager) context.getSystemService(AUDIO_SERVICE);
        if (Build.VERSION.SDK_INT < 31) {
            audio.setSpeakerphoneOn(name.equals("Loudspeaker"));
            return;
        }
        audio.clearCommunicationDevice();
        for (AudioDeviceInfo device : audio.getAvailableCommunicationDevices()) {
            if (name.equals(outputName(device))) {
                audio.setCommunicationDevice(device);
                return;
            }
        }
    }

    public static void callsOver(Context context) {
        manager(context).cancel(CALL_ID);
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (Build.VERSION.SDK_INT >= 26) {
            manager(this).createNotificationChannel(new NotificationChannel(
                RUNNING, "Ready for calls", NotificationManager.IMPORTANCE_LOW));
            manager(this).createNotificationChannel(new NotificationChannel(
                CALLS, "Incoming calls", NotificationManager.IMPORTANCE_HIGH));
        }
        startForeground(RUNNING_ID, builder(this, RUNNING)
            .setContentTitle("Ready for calls")
            .setOngoing(true)
            .build());

        cpu = ((PowerManager) getSystemService(POWER_SERVICE))
            .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "microsipclone:calls");
        cpu.acquire();
        wifi = ((WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE))
            .createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "microsipclone:calls");
        wifi.acquire();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Restarted without the app there would be nothing to keep alive
        return START_NOT_STICKY;
    }

    // The app was swiped away from the recent apps: its window is gone and
    // Qt is shutting down, so stop claiming to be ready for calls
    @Override
    public void onTaskRemoved(Intent rootIntent) {
        stopSelf();
    }

    @Override
    public void onDestroy() {
        cpu.release();
        wifi.release();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private static NotificationManager manager(Context context) {
        return (NotificationManager) context.getSystemService(NOTIFICATION_SERVICE);
    }

    private static PendingIntent openApp(Context context) {
        Intent open = context.getPackageManager()
            .getLaunchIntentForPackage(context.getPackageName());
        return PendingIntent.getActivity(context, 0, open,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }

    private static Notification.Builder builder(Context context, String channel) {
        Notification.Builder builder = Build.VERSION.SDK_INT >= 26
            ? new Notification.Builder(context, channel)
            : new Notification.Builder(context);
        return builder
            .setSmallIcon(android.R.drawable.sym_action_call)
            .setContentIntent(openApp(context));
    }
}
