package net.retropc.pi;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.io.FileInputStream;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.FileOutputStream;
import java.io.BufferedWriter;
import java.io.OutputStreamWriter;

import org.libsdl.app.SDLActivity;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.support.v4.content.ContextCompat;
import android.content.pm.PackageManager;
import android.support.v4.app.ActivityCompat;
import android.widget.Toast;
import android.support.v4.os.EnvironmentCompat;
import android.support.v4.provider.DocumentFile;
import android.os.ParcelFileDescriptor;

public class XM8 extends SDLActivity {
    // log
    private static final String LOG_TAG = "XM8";

    // directory and filename
    private static final String ROM_DIRECTORY = "/XM8/";
    private static final String PC88_FILENAME = "PC88.ROM";
    private static final String N80_FILENAME = "N80.ROM";
    private static final String N88_FILENAME = "N88.ROM";
    private static final String DISK_FILENAME = "DISK.ROM";
    private static final String N88EXT0_FILENAME = "N88_0.ROM";
    private static final String N88EXT1_FILENAME = "N88_1.ROM";
    private static final String N88EXT2_FILENAME = "N88_2.ROM";
    private static final String N88EXT3_FILENAME = "N88_3.ROM";
    private static final String KANJI1_FILENAME = "KANJI1.ROM";

    // message text
    private static final String TOAST_ROM_MESSAGE = "The ROM file is not found: ";
    private static final String TOAST_STORAGE_MESSAGE = "XM8 requires storage permission to access ROM/image files";

    // storage access framework
    private static final String REQUEST_FILENAME = "request.dat";
    private static final String URI_FILENAME = "uri.dat";
    private String mAbsPath;
    private String mExtDir;
    private String mTreeUri;

    // control flag
    private boolean mPermissionError;
    private boolean mROMError;

    // request id
    private static final int REQUEST_PERMISSION = 1;
    private static final int REQUEST_DOCUMENT = 2;

    // JNI native routine
    private native void nativeIntent(String name);
    private native void nativeBuildVer(int ver);
    private native void nativeAbsDir(String dir);
    private native void nativeExtDir(String dir);
    private native void nativeUri(String treeUri);
    private native void nativeSkipMain(int skip);
    private native void nativeDelete();

    // setup
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.i(LOG_TAG, "onCreate");

        // initialize flags
        mPermissionError = false;
        mROMError = false;

        // immersive full-screen mode or dim status bar / navigation icon
        View decorView = getWindow().getDecorView();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        }
        else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
                decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LOW_PROFILE);
            }
        }

        // super class
        super.onCreate(savedInstanceState);

        // set Build.VERSION.SDK_INT and ExternalFilesDir
        nativeBuildVer(Build.VERSION.SDK_INT);
        mAbsPath = getExternalFilesDir(null).getAbsolutePath();
        nativeAbsDir(mAbsPath);

        // process intent
        if (Intent.ACTION_VIEW.equals(getIntent().getAction())) {
            String fname[] = String.valueOf(getIntent().getData()).split("//");
            if (fname.length == 2) {
                nativeIntent(fname[1]);
            }
        }
        else {
            nativeIntent("");
        }
    }

    @Override
    protected void onStart() {
        Log.i(LOG_TAG, "onStart");
        // super class
        super.onStart();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            if (checkStartActivity()) {
                return;
            }

            // read treeUri
            mTreeUri = "";
            try {
                FileInputStream inputStream = openFileInput(URI_FILENAME);
                BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
                mTreeUri = reader.readLine();
                reader.close();
            } catch (java.io.FileNotFoundException e) {
                Log.i(LOG_TAG, "treeUri file is not found");
            } catch (java.io.IOException e) {
                Log.i(LOG_TAG, "treeUri file IO Exception");
            }
            nativeUri(mTreeUri);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // API level 23 or later requires self permission to access storage
            if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                mPermissionError = true;
                Toast.makeText(this, TOAST_STORAGE_MESSAGE, Toast.LENGTH_LONG).show();
                if (!ActivityCompat.shouldShowRequestPermissionRationale(this, android.Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                    // request permission
                    ActivityCompat.requestPermissions(this,  new String[]{android.Manifest.permission.WRITE_EXTERNAL_STORAGE}, REQUEST_PERMISSION);
                }
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String permissions[], int[] grantResults) {
        Log.i(LOG_TAG, "onRequestPermissionResult");
        if (requestCode == REQUEST_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // grant the permission by user operation
                mPermissionError = false;
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        Log.i(LOG_TAG, "onWindowFocusChanged():" + hasFocus);
        if (hasFocus) {
            if (!mPermissionError) {
                // get ROM directory in ExternalStorage
                String basepath = Environment.getExternalStorageDirectory() + ROM_DIRECTORY;

                // check mandatory ROMs
                mROMError = false;
                if (!checkROM(basepath, PC88_FILENAME, false)) {
                    // retry with M88 ROM sets
                    mROMError = false;

                    checkROM(basepath, N80_FILENAME, true);
                    checkROM(basepath, N88_FILENAME, true);
                    checkROM(basepath, DISK_FILENAME, true);
                    checkROM(basepath, N88EXT0_FILENAME, true);
                    checkROM(basepath, N88EXT1_FILENAME, true);
                    checkROM(basepath, N88EXT2_FILENAME, true);
                    checkROM(basepath, N88EXT3_FILENAME, true);
                }
                checkROM(basepath, KANJI1_FILENAME, true);

                // set result to native
                if (mROMError) {
                    nativeSkipMain(1);
                } else {
                    nativeSkipMain(0);
                }

                // get external storage path
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                    String[] extDirs = getExternalStorageDirectories();
                    if (extDirs.length == 1) {
                        // only one path is found
                        mExtDir = extDirs[0];
                        nativeExtDir(mExtDir);
                    }
                }
            }
            else {
                nativeSkipMain(1);
            }
        }

        // super class
        super.onWindowFocusChanged(hasFocus);
    }

    @Override
    protected void onResume() {
        Log.i(LOG_TAG, "onResume");
        // super class
        super.onResume();

        View decorView = getWindow().getDecorView();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        }
        else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
                decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LOW_PROFILE);
            }
        }
    }

    @Override
    protected void onDestroy() {
        Log.i(LOG_TAG, "onDestroy");

        // call DeleteGlobalRef()
        nativeDelete();

        // super class
        super.onDestroy();
    }

    @Override
    protected void onNewIntent(Intent intent) {
        Log.i(LOG_TAG, "onNewIntent()");
        if (Intent.ACTION_VIEW.equals(intent.getAction())) {
            String fname[] = String.valueOf(intent.getData()).split("//");
            if (fname.length == 2) {
                nativeIntent(fname[1]);
            }
        }

        super.onNewIntent(intent);
    }

    // check ROM
    private boolean checkROM(String basepath, String filename, boolean alert) {
        if (mROMError) {
            return false;
        }

        // get result as whether the file exists or not
        File file = new File(basepath + filename);
        boolean result = file.exists();

        // save result
        if (!result) {
            mROMError = true;
        }

        // check result
        if (!result && alert) {
            // toast
            Toast.makeText(this, TOAST_ROM_MESSAGE + basepath + filename, Toast.LENGTH_LONG).show();
        }

        return result;
    }

    // request activity on next launch (called from native)
    public void requestActivity() {
        try {
            FileOutputStream outputStream = this.openFileOutput(REQUEST_FILENAME, MODE_PRIVATE);
            BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(outputStream));
            writer.write(REQUEST_FILENAME);
            writer.flush();
            writer.close();
        } catch (java.io.FileNotFoundException e) {
            Log.i(LOG_TAG, "requestActivity:java.io.FileNotFoundException");
        } catch (java.io.IOException e) {
            Log.i(LOG_TAG, "requestActivity:java.io.IOException");
        }
    }

    // check start activity to grant to access storage
    private boolean checkStartActivity() {
        try {
            FileInputStream inputStream = this.openFileInput(REQUEST_FILENAME);
            Log.i(LOG_TAG, "request file is found, try delete");
            inputStream.close();
            if (deleteFile(REQUEST_FILENAME)) {
                // start activity
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                startActivityForResult(intent, REQUEST_DOCUMENT);
                return true;
            }
        } catch (java.io.FileNotFoundException e) {
            Log.i(LOG_TAG, "request file is not found");
            return false;
        } catch (java.io.IOException e) {
            Log.i(LOG_TAG, "checkStartActivity:java.io.IOException");
            return false;
        }
        return false;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.i(LOG_TAG, "onActivityResult() resultCode:" + resultCode);
        // super class
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == REQUEST_DOCUMENT && resultCode == Activity.RESULT_OK) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                // get persistable permission
                getContentResolver().takePersistableUriPermission(data.getData(),
                        Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            }

            // save Uri to local file
            Log.i(LOG_TAG, "saveUri:" + data.getDataString());
            try {
                FileOutputStream outputStream = this.openFileOutput(URI_FILENAME, MODE_PRIVATE);
                BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(outputStream));
                writer.write(data.getDataString());
                writer.flush();
                writer.close();
            } catch (java.io.FileNotFoundException e) {
                Log.i(LOG_TAG, "saveUri:java.io.FileNotFoundException");
            } catch (java.io.IOException e) {
                Log.i(LOG_TAG, "saveUri:java.io.IOException");
            }
        }
    }

    // clear treeUri
    public void clearTreeUri() {
        try {
            FileInputStream inputStream = this.openFileInput(URI_FILENAME);
            Log.i(LOG_TAG, "uri file is found, try delete");
            inputStream.close();
            if (deleteFile(URI_FILENAME)) {
                mTreeUri = "";
            }
        } catch (java.io.FileNotFoundException e) {
            Log.i(LOG_TAG, "clearTreeUri:java.io.FileNotFoundException");
        } catch  (java.io.IOException e) {
            Log.i(LOG_TAG, "clearTreeUri:java.io.IOException");
        }
    }

    // get file descriptor from treeUri
    public int getFileDescriptor(String file, int type) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            String[] extDirSplit = mExtDir.split("/");
            String[] targetSplit = file.split("/");

            // check count of '/'
            int length = targetSplit.length;
            if (length <= extDirSplit.length) {
                return -1;
            }

            // compare with mExtDir
            int count = 0;
            for (String one : extDirSplit) {
                if (!one.equals(targetSplit[count])) {
                    return -1;
                }
                count++;
            }

            // create DocumentFile from mTreeUri
            Uri treeUri = Uri.parse(mTreeUri);
            DocumentFile treeFile =  DocumentFile.fromTreeUri(this, treeUri);
            if (treeFile == null) {
                return -1;
            }

            // loop DocumentFile.findFile() on each directory
            DocumentFile docFile = null;
            while (count < length) {
                if (docFile == null) {
                    docFile = treeFile.findFile(targetSplit[count]);
                }
                else {
                    docFile = docFile.findFile(targetSplit[count]);
                }
                if (docFile == null) {
                    return -1;
                }
                count++;
            }
            if (docFile == null) {
                return -1;
            }

            // try to open
            String mode = "rw";
            if (type != 0) {
                // read and write access and truncate if file exists
                mode = "rwt";
            }
            try {
                ParcelFileDescriptor pfd = this.getContentResolver().openFileDescriptor(docFile.getUri(), mode);
                int fd = pfd.detachFd();
                Log.i(LOG_TAG,"getFileDescriptor path:" + file + " fd:" + fd);
                return fd;
            } catch (java.io.FileNotFoundException e) {
                Log.i(LOG_TAG, "getFileDescriptor:FileNotFoundException");
            }
        }
        return -1;
    }

    // get external storage path
    // https://stackoverflow.com/questions/36766016/how-to-get-sd-card-path-in-android6-0-programmatically/40205116/#40205116
    private String[] getExternalStorageDirectories() {
        List<String> results = new ArrayList<>();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) { //Method 1 for KitKat & above
            File[] externalDirs = getExternalFilesDirs(null);

            for (File file : externalDirs) {
                String path = file.getPath().split("/Android")[0];

                boolean addPath = false;

                if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                    addPath = Environment.isExternalStorageRemovable(file);
                }
                else{
                    addPath = Environment.MEDIA_MOUNTED.equals(EnvironmentCompat.getStorageState(file));
                }

                if(addPath){
                    results.add(path);
                }
            }
        }

        String[] storageDirectories = new String[results.size()];
        for(int i=0; i<results.size(); ++i) {
            storageDirectories[i] = results.get(i);
        }

        return storageDirectories;
    }
}
