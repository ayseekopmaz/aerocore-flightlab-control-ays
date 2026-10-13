#pragma once
#include <string_view>
namespace aero {
inline bool supportedModel(std::string_view name){return name=="x500"||name=="iha_maker_1"||name=="iha_maker_2"||name=="iha_maker_3";}
}
