//***************************************************************************
//* Copyright (c) 2015 Saint Petersburg State University
//* Copyright (c) 2011-2014 Saint Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#include "config_struct.hpp"

#include "dev_support/openmp_wrapper.h"

#include <yaml-cpp/yaml.h>
#include <string>

namespace YAML {
template<>
struct convert<corrector::Strategy> {
    static bool decode(const YAML::Node &node, corrector::Strategy &rhs) {
        std::string strategy_str = node.as<std::string>();
        if (strategy_str == "all_reads") {
            rhs = corrector::Strategy::AllReads;
            return true;
        } else if (strategy_str == "majority_only") {
            rhs = corrector::Strategy::MajorityOnly;
            return true;
        } else if (strategy_str == "not_started") {
            rhs = corrector::Strategy::AllExceptJustStarted;
            return true;
        } else if (strategy_str == "mapped_squared") {
            rhs = corrector::Strategy::MappedSquared;
            return true;
        }
        return false;
    }
};
}

namespace corrector {
void load(corrector_config& cfg, const std::string &filename) {
    YAML::Node config = YAML::LoadFile(filename);
    cfg.dataset.load(config["dataset"].as<std::string>());
    cfg.work_dir = config["work_dir"].as<std::string>(".");
    cfg.output_dir = config["output_dir"].as<std::string>(".");
    cfg.max_nthreads = config["max_nthreads"].as<unsigned>();

    cfg.max_nthreads = std::min(cfg.max_nthreads, (unsigned) omp_get_max_threads());
    cfg.strat = config["strategy"].as<Strategy>();
    cfg.bwa = config["bwa"].as<std::string>(".");
    omp_set_num_threads(cfg.max_nthreads);
}
}
