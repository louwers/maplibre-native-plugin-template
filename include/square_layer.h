#pragma once

// Public entry point of the square layer plugin. Applications (or the generated
// Android/iOS wrappers) call it once, before loading a style that uses
// `"type": "square"` layers.
#include <mln/plugin/plugin_api.h>

#if defined(_WIN32)
#define MLN_SQUARE_LAYER_EXPORT __declspec(dllexport)
#else
#define MLN_SQUARE_LAYER_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Registers the `square` layer type with MapLibre.
///
/// `register_plugin` is MapLibre's `mln_plugin_register_v1`. Passing it in, rather than
/// calling it directly, keeps the plugin independent of how the host exposes it.
/// Returns MLN_PLUGIN_STATUS_OK, or MLN_PLUGIN_STATUS_ALREADY_REGISTERED on repeated calls.
MLN_SQUARE_LAYER_EXPORT mln_plugin_status mln_square_layer_register(mln_plugin_register_function_v1 register_plugin,
                                                                    char* error_message,
                                                                    size_t error_message_capacity);

#ifdef __cplusplus
}
#endif
