#pragma once

#include <mbgl/plugin/plugin_api.h>

extern "C" mln_plugin_status mln_gltf_layer_register(mln_plugin_register_function_v1,
                                                      char* error_message,
                                                      size_t error_message_capacity);
