#include <jni.h>

#include <stdint.h>

#include "shadow_renderer.hpp"

namespace {

jobject makeNativeResult(JNIEnv* env, mln_plugin_status status, const char* message) {
    jclass type = env->FindClass("org/maplibre/plugins/shadows/FillExtrusionShadowsPlugin$NativeResult");
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
Java_org_maplibre_plugins_shadows_FillExtrusionShadowsPlugin_nativeRegister(JNIEnv* env,
                                                                            jclass,
                                                                            jlong functionAddress) {
    char error[512]{};
    if (!functionAddress) {
        return makeNativeResult(env, MLN_PLUGIN_STATUS_NOT_FOUND, "MapLibre plugin registration function is unavailable");
    }
    const auto registerPlugin = reinterpret_cast<mln_plugin_register_function_v1>(
        static_cast<uintptr_t>(functionAddress));
    const auto status = mln_fill_extrusion_shadows_register(registerPlugin, error, sizeof(error));
    return makeNativeResult(env, status, error);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_maplibre_plugins_shadows_FillExtrusionShadowsPlugin_nativeRenderCallbackCount(JNIEnv*, jclass) {
    return static_cast<jlong>(mln_fill_extrusion_shadows_render_callback_count());
}
