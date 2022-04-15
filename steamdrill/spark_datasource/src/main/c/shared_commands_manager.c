#include <cassert>
#include <cerrno>
#include <cstring>
#include <jni.h>

#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "jni_ShmIter.h"

#ifdef __cplusplus
extern "C" {
#endif

struct file_header {
    int num_commands;
};


// function that gets whatever jfield Id is for this thing?
jfieldID getMeta(JNIEnv *env, jobject that) {

  static jfieldID metadataField = 0;
  if (!metadataField) {
    jclass c = env->GetObjectClass(that);
    metadataField = env->GetFieldID(c, "metadata", "Ljava/lang/String;"); // not sure what 'J' is?
    env->DeleteLocalRef(c); // guess this frees the mem?
  }
  return metadataField;
}

// function that gets whatever jfield Id is for this thing?
jfieldID getFD(JNIEnv *env, jobject that) {

  static jfieldID fdField = 0;
  if (!fdField) {
    jclass c = env->GetObjectClass(that);
    fdField = env->GetFieldID(c, "fd", "I"); // not sure what 'J' is?
    env->DeleteLocalRef(c); // guess this frees the mem?
  }
  return fdField;
}


/*
 * Class:     jni_SharedCommandsManager
 * Method:    openAndLock
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_jni_SharedCommandsManager_openAndLock(JNIEnv *env, jobject that) {
    fprintf(stderr, "started open_and_lock! \n");
    jstring meta = (jstring)env->GetObjectField(that, getMeta(env, that));
    const char *metadata = env->GetStringUTFChars(meta, 0);
    int fd, rc;

    fprintf(stderr, "creating and locking %s\n", metadata);
    // get/create the metadata file. I guess this sould go in shmem. OH WELL LOL.
    fd = open(metadata, O_RDWR | O_CREAT, 0777);
    if (fd < 0) {
        fprintf(stderr, "cannot open %s: error %d(%s)", metadata, errno, strerror(errno));
    }
    // lock the file:
    rc = flock(fd, LOCK_EX);
    if (rc) {
        fprintf(stderr, "cannot flock the file: %s", strerror(errno));
    }

    env->SetIntField(that, getFD(env, that), (jint)fd);
    env->ReleaseStringUTFChars(meta, metadata);
}


/*
 * Class:     jni_SharedCommandsManager
 * Method:    addCommands
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_jni_SharedCommandsManager_addCommands (JNIEnv *env, jobject that , jstring comm) {

    const char *commands = env->GetStringUTFChars(comm, 0);
    int fd = env->GetIntField(that, getFD(env, that));
    int rc, command_size = strlen(commands);
    struct file_header header;
    struct stat stat_buf;

    // either create or update the header and set to the next free bye in the file
    rc = fstat(fd, &stat_buf);
    assert (rc == 0);

    if (stat_buf.st_size == 0) {
        header.num_commands = 1;
        rc = write(fd, &header, sizeof(header));
        assert (rc == sizeof(header));
    }
    else {
        // This isn't *really* the best approach to doing this. But, OH WELL.
        rc = read(fd, &header, sizeof(header));
        if (rc != sizeof(header)) {
            fprintf(stderr, "cannot read the file_header!?");
        }
        header.num_commands += 1;
        rc = lseek(fd,  0, SEEK_SET);
        assert (rc == 0);
        rc = write(fd, &header, sizeof(header));
        assert (rc == sizeof(header));

        rc = lseek(fd, stat_buf.st_size, SEEK_SET);
        assert (rc == stat_buf.st_size);
    }
    fprintf(stderr, "%d commmands now\n", header.num_commands);

    // write the next free byte.
    rc = write(fd, &command_size, sizeof(command_size));
    assert (rc == sizeof(command_size));
    rc = write(fd, commands, command_size);
    assert (rc == command_size);

    env->ReleaseStringUTFChars(comm, commands);
    return header.num_commands;
}



/*
 * Class:     jni_SharedCommandsManager
 * Method:    getCommands
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_jni_SharedCommandsManager_getCommands (JNIEnv *env, jobject that) {
    int fd = env->GetIntField(that, getFD(env, that));
    int rc, done, size;
    struct file_header header;
    jobjectArray result;
    jclass stringCls = env->FindClass("java/lang/String");

    if (stringCls == NULL) {
        fprintf(stderr, "couldn't find string??\n");
    }

    rc = lseek(fd,  0, SEEK_SET);
    assert (rc == 0);

    rc = read(fd, &header, sizeof(header));
    assert (rc == sizeof(header));
    fprintf(stderr, "Creating a new array of size %d\n", header.num_commands);
    result = env->NewObjectArray(header.num_commands, stringCls, env->NewStringUTF(""));

    for (int i = 0; i < header.num_commands; ++i) {
        char nextItem[512];
        jstring str;
        int rc, size;

        rc = read(fd, &size, sizeof(size));
        assert (rc == sizeof(size));
        assert (size < 512);
        rc = read(fd, nextItem, size);
        assert (rc == size);
        nextItem[size] = 0;
        str = env->NewStringUTF(nextItem);
        env->SetObjectArrayElement(result, i, str);
    }
    return result;
  }

/*
 * Class:     jni_SharedCommandsManager
 * Method:    closeAndUnlock
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_jni_SharedCommandsManager_closeAndUnlock (JNIEnv *env, jobject that) {
    int fd = env->GetIntField(that, getFD(env, that));
    // clean up!
    int rc = flock(fd, LOCK_UN);
    if (rc != 0) {
        fprintf(stderr, "couldn't unlock error=%d(%s)", errno, strerror(errno));
    }
    rc = close(fd);
    assert (rc == 0);
}

/*
* Class:     jni_SharedCommandsManager
* Method:    clearCommands
* Signature: (Ljava/lang/String;)V
*/
JNIEXPORT void JNICALL Java_jni_SharedCommandsManager_clear (JNIEnv *env, jobject that) {
    jstring meta = (jstring)env->GetObjectField(that, getMeta(env, that));
    const char *metadata = env->GetStringUTFChars(meta, 0);
    int rc = unlink(metadata);
    if (rc) {
        fprintf(stderr, "couldn't unlink %s, errno %d (%s)\n", metadata, errno, strerror(errno));
    }
    env->ReleaseStringUTFChars(meta, metadata);
}


#ifdef __cplusplus
}
#endif
