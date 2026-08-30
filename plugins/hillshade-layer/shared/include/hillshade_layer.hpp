#pragma once

#include <mln/plugin/plugin_api.h>

#include <stddef.h>

#if defined(_WIN32)
#define MLN_HILLSHADE_EXPORT __declspec(dllexport)
#else
#define MLN_HILLSHADE_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

MLN_HILLSHADE_EXPORT mln_plugin_status
mln_hillshade_layer_register(mln_plugin_register_function_v1 register_plugin,
                             char* error_message,
                             size_t error_message_capacity);

#ifdef __cplusplus
}
#endif
