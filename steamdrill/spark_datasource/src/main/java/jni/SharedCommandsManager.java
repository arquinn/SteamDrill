package jni;

public class SharedCommandsManager {
    String metadata;
    int fd; // file descriptor of the metadata file



    public SharedCommandsManager(String meta) {
        // unfortunate name
        System.loadLibrary("shm_iter");
        metadata = meta;
        openAndLock();
    }

    private native void openAndLock();

    public native int addCommands(String commands);
    public native String[] getCommands();
    public native void closeAndUnlock();
    public native void clear();
}
