
#include <cassert>
#include <cerrno>
#include <cstring>
#include <jni.h>

#include <unistd.h>

extern "C" {
#include "shared_state.h"
}

#include "jni_ShmIter.h"


//#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#define TRACE(x,...)

static jfieldID ptrField = 0;
static jfieldID countField = 0;
static jfieldID joinField = 0;
static jfieldID dataField = 0;

static jclass objectClass = NULL;
static jclass integerClass = NULL;
static jclass longClass = NULL;
static jclass shortClass = NULL;

static jmethodID integerConstructor = 0;
static jmethodID longConstructor = 0;
static jmethodID shortConstructor = 0;
static jmethodID stringConstructor = 0;



// function that gets whatever jfield Id is for this thing?
void initGetPtr(JNIEnv *env, jobject that) {
  if (!ptrField) {
    jclass c = env->GetObjectClass(that);
    ptrField = env->GetFieldID(c, "ptr", "J"); // not sure what 'J' is?
    env->DeleteLocalRef(c); // guess this frees the mem?
  }
}

// function that gets whatever jfield Id is for this thing?
void initGetCount(JNIEnv *env, jobject that) {
  if (!countField) {
    jclass c = env->GetObjectClass(that);
    countField = env->GetFieldID(c, "count", "J"); // not sure what 'J' is?
    env->DeleteLocalRef(c); // guess this frees the mem?
  }
}


// function that gets whatever jfield Id is for this thing?
void initGetUsesJoin(JNIEnv *env, jobject that) {
  if (!joinField) {
    jclass c = env->GetObjectClass(that);
    joinField = env->GetFieldID(c, "joinBytes", "I");
    env->DeleteLocalRef(c);
  }
}


// function that gets whatever jfield Id is for this thing?
void initGetDataTypes(JNIEnv *env, jobject that) {
  if (!dataField) {
    jclass c = env->GetObjectClass(that);
    dataField = env->GetFieldID(c, "types", "[Ljava/lang/String;");
    env->DeleteLocalRef(c); // guess this frees the mem?
  }
}

void initTypes(JNIEnv *env) { 
	jclass tempRef = env->FindClass("java/lang/Object");
	objectClass = (jclass) env->NewGlobalRef(tempRef);
	env->DeleteLocalRef(tempRef);

	tempRef = env->FindClass("java/lang/Integer");
	integerClass = (jclass) env->NewGlobalRef(tempRef);
	env->DeleteLocalRef(tempRef);

	tempRef = env->FindClass("java/lang/Short");
	shortClass = (jclass) env->NewGlobalRef(tempRef);
	env->DeleteLocalRef(tempRef);

	tempRef = env->FindClass("java/lang/Long");
	longClass = (jclass) env->NewGlobalRef(tempRef);
	env->DeleteLocalRef(tempRef);

	integerConstructor = env->GetMethodID(integerClass, "<init>", "(I)V");
	shortConstructor = env->GetMethodID(shortClass, "<init>", "(S)V");
	longConstructor = env->GetMethodID(longClass, "<init>", "(J)V");
}

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Class:     jni_ShmIter
 * Method:    initialize
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_jni_ShmIter_initialize(JNIEnv *env, jobject that, jstring filename) {

	  initGetPtr(env, that);
		initGetUsesJoin(env, that);
		initGetDataTypes(env, that);
    initGetCount(env, that);
		initTypes(env);

    const char *name = env->GetStringUTFChars(filename, 0);
    struct shared_state *s = shared_reader(name);
    if (!s) {
        fprintf(stderr,"hmm, open_reader returned null...???\n");
        assert (false);
    }
    // fprintf(stderr, "open, %s\n", name);
    env->SetLongField(that, ptrField, (jlong)s);
    env->SetLongField(that, countField, (jlong)0);
    env->ReleaseStringUTFChars(filename, name);
}

/*
 * Class:     jni_ShmIter
 * Method:    getCreated
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_jni_ShmIter_getCreated(JNIEnv *env, jobject that) {
    struct shared_state *s = (struct shared_state*)env->GetLongField(that, ptrField);
    assert (s != NULL);
    return s->created ? JNI_TRUE : JNI_FALSE;
}

/*
 * Class:     jni_ShmIter
 * Method:    getFd
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_jni_ShmIter_getFd (JNIEnv *env, jobject that) {
    struct shared_state *s = (struct shared_state*)env->GetLongField(that, ptrField);
    assert (s != NULL);
    return s->fd;
}


/*
 * Class:     jni_ShmIter
 * Method:    nativeNext
 * Signature: ()[Ljava/lang/Object;
 */
JNIEXPORT jobjectArray JNICALL Java_jni_ShmIter_nativeNext (JNIEnv *env, jobject that) {
	  TRACE(stderr, "native Next start\n");

    struct shared_state *s = (struct shared_state*)env->GetLongField(that, ptrField);
    jint usesJoin = env->GetIntField(that, joinField);

    jobjectArray result = NULL;
    TRACE(stderr, "sharedState=%p\n", s);

    long count = env->GetLongField(that, countField);

    jobjectArray typeArray = (jobjectArray) env->GetObjectField(that, dataField);
    TRACE(stderr, "typeArray=%p\n", typeArray);
    jsize length = env->GetArrayLength((jarray)typeArray);
    TRACE(stderr, "native Next: datatypes length %d\n", length);

    // check that we've waited and that we're finished:
    // first, check if we're done:
    shared_wait(s);
    if (!shared_isFinished(s, count)) {
			  TRACE(stderr, "unfinished business! we use join? %d, obj class %p\n", usesJoin, objectClass);

        if (usesJoin)
            result = env->NewObjectArray(length + 1, objectClass, NULL);
        else
            result = env->NewObjectArray(length, objectClass, NULL);

        for (int i = 0; i < length; ++i) {
            jstring stringObj = (jstring) env->GetObjectArrayElement(typeArray, i);
            const char *rawString = env->GetStringUTFChars(stringObj, 0);
            if (rawString[0] == 'i') {
                jint val = shared_getInt(s);
								TRACE(stderr, "next int! %x\n", val);
                jobject obj = env->NewObject(integerClass, integerConstructor, val);
                env->SetObjectArrayElement(result, i, obj);
            }	else if (rawString[0] == 'l') {
							// java doesn't support unsigned Ints, so we use longs as unsigned ints.
							// but, we notably DON'T support regular longs (now). Which might be bad long term.
							  jlong val = (long long)(u_long)shared_getInt(s);
								TRACE(stderr, "next long! %llx\n", val);
                jobject obj = env->NewObject(longClass, longConstructor, val);
								env->SetObjectArrayElement(result, i, obj);
						} else if (rawString[0] == 's' && rawString[1] == 'h') {
							  jshort val = shared_getShort(s);
								TRACE(stderr, "next short! %x\n", val);
                jobject obj = env->NewObject(shortClass, shortConstructor, val);
                env->SetObjectArrayElement(result, i, obj);
            } else if (rawString[0] == 's' && rawString[1] == 't') {
							  char *string = shared_getString(s);
                TRACE(stderr, "next string! %s\n", string);
                jstring str = env->NewStringUTF(string);

                env->SetObjectArrayElement(result, i, str);
             }
            env->ReleaseStringUTFChars(stringObj, rawString);
        }
        if (usesJoin) {
          jobject obj = NULL;
          jint val = usesJoin == 2 ?
              (jint)(u_short)shared_getShort(s) :
              shared_getInt(s);
          obj = env->NewObject(integerClass, integerConstructor, val);
          env->SetObjectArrayElement(result, length, obj);
        }
    }

    env->SetLongField(that, countField, (jlong)(count + 1));
    return result;
}



/*
 * Class:     jni_ShmIter
 * Method:    closeJNI
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_jni_ShmIter_closeJNI(JNIEnv * env, jobject that, jstring filename) {
    const char *name = env->GetStringUTFChars(filename, 0);

    struct shared_state *s = (struct shared_state*)env->GetLongField(that, ptrField);
    // fprintf(stderr, "close, %s\n", name);
    shared_close(s);
    /*
			// because of how this now works, maybe we can actually close here? 
    int rc = unlink(name);
    if (rc) {
        fprintf(stderr, "couldn't unlink %s, errno %d\n", name, errno);
    }
    */
    env->ReleaseStringUTFChars(filename, name);
}


#ifdef __cplusplus
}
#endif
