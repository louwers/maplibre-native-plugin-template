#include <jni.h>

#include <cstdint>

#include "hillshade_layer.hpp"

namespace {

jobject makeResult(JNIEnv* env, mln_plugin_status status, const char* message) {
    jclass type = env->FindClass("org/maplibre/plugins/hillshade/HillshadeLayerPlugin$NativeResult");
    if (!type) return nullptr;
    jmethodID constructor = env->GetMethodID(type, "<init>", "(ILjava/lang/String;)V");
    if (!constructor) return nullptr;
    jstring javaMessage = env->NewStringUTF(message ? message : "");
    jobject result = env->NewObject(type, constructor, static_cast<jint>(status), javaMessage);
    env->DeleteLocalRef(javaMessage);
    env->DeleteLocalRef(type);
    return result;
}

} // namespace

extern "C" JNIEXPORT jobject JNICALL
Java_org_maplibre_plugins_hillshade_HillshadeLayerPlugin_nativeRegister(JNIEnv* env,
                                                                        jclass,
                                                                        jlong functionAddress) {
    char error[512]{};
    if (!functionAddress) {
        return makeResult(env, MLN_PLUGIN_STATUS_NOT_FOUND, "MapLibre plugin registration function is unavailable");
    }
    const auto registerPlugin =
        reinterpret_cast<mln_plugin_register_function_v1>(static_cast<uintptr_t>(functionAddress));
    const auto status = mln_hillshade_layer_register(registerPlugin, error, sizeof(error));
    return makeResult(env, status, error);
}
