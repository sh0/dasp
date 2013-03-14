#ifndef DASP_EVAL
#define DASP_EVAL

#include <dasp/Superpixels.hpp>
#include <Slimage/Slimage.hpp>
#include <map>
#include <vector>

namespace dasp {
namespace eval {


std::pair<float,std::vector<float>> IsoperimetricQuotient(const Superpixels& u);

float ExplainedVariationColor(const Superpixels& sp);

float ExplainedVariationDepth(const Superpixels& sp);

float ExplainedVariationPosition(const Superpixels& sp);

float ExplainedVariationNormal(const Superpixels& sp);

}
}

#endif
