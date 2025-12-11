package com.example.androidplayer;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.provider.Settings;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity {
    private static final int REQUEST_MANAGE_ALL_FILES_ACCESS_PERMISSION = 1001;

    private Player player;
    private Handler mHandler;
    private SeekBar mSeekBar;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Android 11+申请所有文件访问权限
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                startActivityForResult(intent, REQUEST_MANAGE_ALL_FILES_ACCESS_PERMISSION);
            }
        }

        setContentView(R.layout.activity_main);

        mSeekBar = findViewById(R.id.seekBar);

        mHandler = new Handler(Looper.getMainLooper()) {
            public void handleMessage(@NonNull Message msg) {
                if (msg.what == 1) {
                    int progress = msg.getData().getInt("progress");
                    mSeekBar.setProgress(progress);
                }
            }
        };

        player = new Player();
        player.setDataSource("/sdcard/1.mp4");

        SurfaceView surfaceView = findViewById(R.id.surfaceView);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                player.setSurface(holder.getSurface());
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
            }
        });

        Thread progressThread = new Thread(() -> {
            while (true) {
                int progress = (int) Math.round(player.getProgress() * 100);
                Message msg = new Message();
                Bundle bundle = new Bundle();
                bundle.putInt("progress", progress);
                msg.setData(bundle);
                msg.what = 1;
                mHandler.sendMessage(msg);
                try {
                    Thread.sleep(500);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                    break;
                }
            }
        });

        Button play = findViewById(R.id.button);
        play.setOnClickListener(v -> {
            switch (player.getState()) {
                case None:
                case End:
                    player.start();
                    if (!progressThread.isAlive())
                        progressThread.start();
                    play.setText("暂停");
                    break;
                case Playing:
                    player.pause(true);
                    play.setText("播放");
                    break;
                case Paused:
                    player.pause(false);
                    play.setText("暂停");
                    break;
                default:
                    break;
            }
        });

        Button stop = findViewById(R.id.button2);
        stop.setOnClickListener(v -> {
            player.stop();
            play.setText("播放");
            mSeekBar.setProgress(0);
        });

        Button speed = findViewById(R.id.button3);
        speed.setText("1x");
        speed.setOnClickListener(v -> {
            switch (speed.getText().toString()) {
                case "2x":
                    player.setSpeed(3);
                    speed.setText("3x");
                    break;
                case "3x":
                    player.setSpeed(0.5f);
                    speed.setText("0.5x");
                    break;
                case "0.5x":
                    player.setSpeed(1);
                    speed.setText("1x");
                    break;
                case "1x":
                    player.setSpeed(2);
                    speed.setText("2x");
                    break;
            }
        });

        mSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser)
                    player.seek((double) progress / 100);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        Button decodeButton = findViewById(R.id.button_decode);
        decodeButton.setOnClickListener(v -> {
            // 检查是否具备权限
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
                    !Environment.isExternalStorageManager()) {
                Toast.makeText(this, "请先授权所有文件访问权限", Toast.LENGTH_SHORT).show();
                Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                startActivityForResult(intent, REQUEST_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                return;
            }

            // 子线程解码
            new Thread(() -> {
                String inputPath = "/sdcard/1.mp4";
                String outputPath = "/sdcard/output.yuv";
                player.startDecode(inputPath, outputPath);
                runOnUiThread(() -> Toast.makeText(this, "YUV解码保存完成", Toast.LENGTH_SHORT).show());
            }).start();
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_MANAGE_ALL_FILES_ACCESS_PERMISSION) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                if (Environment.isExternalStorageManager()) {
                    Toast.makeText(this, "已获得全部文件访问权限", Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(this, "未获得权限，功能可能受限", Toast.LENGTH_SHORT).show();
                }
            }
        }
        super.onActivityResult(requestCode, resultCode, data);
    }
}
