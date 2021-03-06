#ifndef MOLECULE_EXTRACTION_STAGE_HPP
#define MOLECULE_EXTRACTION_STAGE_HPP

// #include "molecule_extraction_stage.hpp"
#include "io/dataset_support/dataset_readers.hpp"
#include "assembly_graph/paths/bidirectional_path_io/bidirectional_path_output.hpp"
#include "assembly_graph/paths/bidirectional_path_io/io_support.hpp"

namespace debruijn_graph {
    typedef io::SequencingLibrary<debruijn_graph::config::DataSetData> lib_t;
    typedef std::vector<io::SequencingLibrary<debruijn_graph::config::DataSetData>> lib_vector_t;


    bool Has10XRNAReads(config::dataset ds) {
        bool has_10X_RNA = false;
        for (const auto& lib: ds.reads) {
            if (lib.type() == io::LibraryType::RNA10x) {
                has_10X_RNA = true;
            }
        }
        return has_10X_RNA;
    }

    class SimplePathExtractor {
    private:
        const debruijn_graph::conj_graph_pack &gp_;
        std::set<EdgeId> used_edges_;
        path_extend::PathContainer temp_set_;

        std::vector<EdgeId> getForwardIntersect(EdgeId e, const std::set<EdgeId> &edge_set) const {
            auto edges = gp_.g.IncidentEdges(gp_.g.EdgeEnd(e));
            std::vector<EdgeId> filtered;
            for (auto temp_e : edges) {
                if (temp_e != e && edge_set.count(temp_e) && !used_edges_.count(temp_e)) {
                    filtered.push_back(temp_e);
                }
            }
            return filtered;
        }

        std::vector<EdgeId> getReverseIntersect(EdgeId e, const std::set<EdgeId> &edge_set) const {
            auto edges = gp_.g.IncidentEdges(gp_.g.EdgeStart(e));
            std::vector<EdgeId> filtered;
            for (auto temp_e : edges) {
                if (temp_e != e && edge_set.count(temp_e) && !used_edges_.count(temp_e)) {
                    filtered.push_back(temp_e);
                }
            }
            return filtered;
        }

        std::vector<EdgeId> getForwardIntersect(EdgeId e, const GraphComponent<Graph> &comp) const {

            auto edges = gp_.g.IncidentEdges(gp_.g.EdgeEnd(e));
            DEBUG("Edges " << edges);
            std::vector<EdgeId> filtered;
            for (auto temp_e : edges) {
                if (temp_e != e && comp.edges().count(temp_e)) {
                    filtered.push_back(temp_e);
                }
            }
            DEBUG("Filtered " << filtered);
            return filtered;
        }

        std::vector<EdgeId> getReverseIntersect(EdgeId e, const GraphComponent<Graph> &comp) const {
            auto edges = gp_.g.IncidentEdges(gp_.g.EdgeStart(e));
            std::vector<EdgeId> filtered;
            for (auto temp_e : edges) {
                if (temp_e != e && comp.edges().count(temp_e)) {
                    filtered.push_back(temp_e);
                }
            }
            return filtered;
        }

        void extendForward(const std::set<EdgeId> &edge_set,
                           std::deque<EdgeId> &linear_path, EdgeId e) {
            auto extensions = getForwardIntersect(e, edge_set);
            for (auto next_edge : extensions) {
                linear_path.push_back(next_edge);
                used_edges_.insert(e);
                used_edges_.insert(gp_.g.conjugate(e));
                extendForward(edge_set, linear_path, next_edge);
                extendBackward(edge_set, linear_path, next_edge);
            }
        }


        void extendBackward(const std::set<EdgeId> &edge_set,
                            std::deque<EdgeId> &linear_path, EdgeId e) {
            auto extensions = getReverseIntersect(e, edge_set);
            for (auto prev_edge : extensions) {
                linear_path.push_front(prev_edge);
                used_edges_.insert(prev_edge);
                used_edges_.insert(gp_.g.conjugate(prev_edge));
                extendForward(edge_set, linear_path, prev_edge);
                extendBackward(edge_set, linear_path, prev_edge);
            }
        }

        void extendForward(const GraphComponent<Graph> &comp,
                           std::deque<EdgeId> &linear_path, std::set<EdgeId> &used_edges) {
            EdgeId e = linear_path.back();
            if (comp.VertexInDegree(gp_.g.EdgeEnd(e)) != 1 ||
                    comp.VertexOutDegree(gp_.g.EdgeEnd(e)) != 1 ) {
                return;
            }
            auto extensions = getForwardIntersect(e, comp);

            VERIFY(extensions.size() == 1);
            EdgeId next_edge = extensions[0];
            if (used_edges.count(next_edge)) {
                return;
            }
            linear_path.push_back(next_edge);
            used_edges.insert(next_edge);
            used_edges.insert(gp_.g.conjugate(next_edge));
            extendForward(comp, linear_path, used_edges);
        }


        void extendBackward(const GraphComponent<Graph> &comp,
                            std::deque<EdgeId> &linear_path, std::set<EdgeId> &used_edges) {
            EdgeId e = linear_path.front();
            if (comp.VertexInDegree(gp_.g.EdgeStart(e)) != 1 ||
                    comp.VertexOutDegree(gp_.g.EdgeStart(e)) != 1 ) {
                return;
            }
            auto extensions = getReverseIntersect(e, comp);

            VERIFY(extensions.size() == 1);
            EdgeId prev_edge = extensions[0];
            if (used_edges.count(prev_edge)) {
                return;
            }
            linear_path.push_front(prev_edge);
            used_edges.insert(prev_edge);
            used_edges.insert(gp_.g.conjugate(prev_edge));
            extendBackward(comp, linear_path, used_edges);
        }

        //Here we suppose that this is single connected component
        bool IsSimplePath(const GraphComponent<Graph> &comp) const {
            size_t one_degree = 0;
            for (auto v : comp.vertices()) {
                auto edges = comp.IncidentEdges(v);
                VERIFY(edges.size() > 0);
                if (edges.size() >= 3) {
                    return false;
                }
                if (edges.size() == 1) {
                    one_degree++;
                }
            }
            return one_degree == 4;
        }

        //Here we suppose that this is single connected component
        bool IsSimpleCycle(const GraphComponent<Graph> &comp) const {
            return comp.vertices().size() == 2 && comp.edges().size() == 2;
        }

        size_t SplitComponentsOnSimplePaths(const GraphComponent<Graph> &comp) {
            size_t result = 0;
            //check self-conjugate
            set<EdgeId> used_edges;
            for (auto e : comp.edges()) {
                if (gp_.g.conjugate(e) == e) {
                    used_edges.insert(e);
                }
                if (gp_.g.EdgeEnd(e) == gp_.g.EdgeStart(e)) {
                    used_edges.insert(e);
                }
            }


            DEBUG(comp.edges());
            for (auto e : comp.edges()) {
                if (!used_edges.count(e)) {
                    DEBUG("Current edge " << e);
                    used_edges.insert(e);
                    used_edges.insert(gp_.g.conjugate(e));
                    std::deque<EdgeId> linear_path;
                    linear_path.push_back(e);
                    extendForward(comp, linear_path, used_edges);
                    extendBackward(comp, linear_path, used_edges);
                    path_extend::BidirectionalPath *path = new path_extend::BidirectionalPath(gp_.g);
                    auto conj = new path_extend::BidirectionalPath(gp_.g);
                    temp_set_.AddPair(path, conj);
                    for (auto e : linear_path) {
                        path->PushBack(e);
                    }
                    DEBUG("Add path");
                    path->PrintDEBUG();
                    result++;
                }
            }
            return result;
        }

        void RemoveBulges(GraphComponent<Graph> &comp) {
            SplitComponentsOnSimplePaths(comp);


            vector<path_extend::BidirectionalPath *> paths;

            for (auto p : temp_set_) {
                paths.push_back(p.first);
                paths.push_back(p.second);
            }

            vector<size_t> to_remove;

            for (size_t i = 0; i < paths.size(); ++i) {
                for (size_t j = i + 1; j < paths.size(); ++j) {


                    if (gp_.g.EdgeStart(paths[i]->Front()) == gp_.g.EdgeStart(paths[j]->Front()) &&
                            gp_.g.EdgeEnd(paths[i]->Back()) == gp_.g.EdgeEnd(paths[j]->Back())) {
                        paths[i]->PrintDEBUG();
                        paths[j]->PrintDEBUG();
                        if (paths[i]->GetConjPath() == paths[j]) {
                            DEBUG("Self-conjugate bulges");
                            continue;
                        }

                        if (paths[i]->Coverage() > paths[j]->Coverage()) {
                            to_remove.push_back(j);
                        } else {
                            to_remove.push_back(i);
                        }

                    }

                    if (gp_.g.EdgeStart(paths[i]->Front()) == gp_.g.EdgeEnd(paths[j]->Back()) &&
                        gp_.g.EdgeEnd(paths[i]->Back()) == gp_.g.EdgeStart(paths[j]->Front())) {
                        paths[i]->PrintDEBUG();
                        paths[j]->PrintDEBUG();
                        if (paths[i]->Coverage() > paths[j]->Coverage()) {
                            to_remove.push_back(j);
                        } else {
                            to_remove.push_back(i);
                        }

                    }
                }
            }


            for (auto i : to_remove) {
                for (size_t j = 0; j < paths[i]->Size(); ++j) {
                    comp.RemoveEdge((*(paths[i]))[j]);
                }
            }

        }


    public:
        SimplePathExtractor(const debruijn_graph::conj_graph_pack &gp)
        : gp_(gp) {
        }

        path_extend::PathContainer& getLongReads(std::set<EdgeId> &edge_set) {
            temp_set_.clear();
            std::set<EdgeId> bad_edges;
            auto initial_component = GraphComponent<Graph>::FromEdges(gp_.g, edge_set, true);
            DEBUG("Initial component");
            DEBUG(initial_component.edges());

            for (size_t i = 0; i < 2; ++i) {
                initial_component.RemoveIsolated();
                DEBUG("After remove isolated");
                initial_component.ClipTips();
                DEBUG("After clip tips");
                DEBUG(initial_component.edges());

                initial_component.FillGaps(30 * (i + 1));
                DEBUG("After fill gaps");
                DEBUG(initial_component.edges());
                RemoveBulges(initial_component);
                DEBUG("After remove bulges");
                DEBUG(initial_component.edges());
                initial_component.RemoveLowCoveredJunctions();
                temp_set_.clear();
            }
            DEBUG("Fixed initial component");
            DEBUG(initial_component.edges());

            edge_set = initial_component.edges();
            for (auto e : edge_set) {
                if (!used_edges_.count(e)) {

                    std::deque<EdgeId> linear_path;
                    linear_path.push_back(e);
                    used_edges_.insert(e);
                    used_edges_.insert(gp_.g.conjugate(e));
                    extendForward(edge_set, linear_path, e);
                    extendBackward(edge_set, linear_path, e);
                    auto component = GraphComponent<Graph>::FromEdges(gp_.g, linear_path, true);
                    component.ClipTips();

                    if (IsSimplePath(component)) {
                        DEBUG("Component is a simple path");
                        DEBUG(component.edges());
                        path_extend::BidirectionalPath *path = new path_extend::BidirectionalPath(gp_.g);
                        path->SetConjPath(new path_extend::BidirectionalPath(gp_.g));
                        for (auto e : linear_path) {
                            path->PushBack(e);
                        }
                        temp_set_.AddPair(path, path->GetConjPath());
                    } else if (IsSimpleCycle(component)) {
                        DEBUG("Component is a simple cycle");
                        DEBUG(component.edges());
                    } else {
                        DEBUG("Component is not a simple path");
                        DEBUG(component.edges());
                        size_t result = SplitComponentsOnSimplePaths(component);
                        DEBUG("Component is split on " << result << " paths");
                    }
                }
            }
            used_edges_.clear();
            return temp_set_;
        }

    };

    class LongReadsCreator {
    private:
        const debruijn_graph::conj_graph_pack &gp_;
        SimplePathExtractor extractor_;

    public:
        LongReadsCreator(debruijn_graph::conj_graph_pack &gp)
        : gp_(gp), extractor_(gp) {
        }

        void extractLongReads(std::vector<MappingPath<EdgeId>> &paths, path_extend::PathContainer &path_set, std::string &barcode) {
            std::set<EdgeId> edge_set;
            for (auto const& path : paths) {
                std::vector<EdgeId> edges = path.simple_path();
                for (auto e : edges) {
                    edge_set.insert(e);
                    edge_set.insert(gp_.g.conjugate(e));
                }
            }
            DEBUG("Number of edges - " << edge_set.size() / 2);
            auto &temp_set = extractor_.getLongReads(edge_set);
            for (auto path_pair : temp_set) {
                path_pair.first->barcode = barcode;
                path_pair.second->barcode = barcode;

                path_set.AddPair(path_pair.first, path_pair.second);
            }
            temp_set.clear();
        }
    };



    std::string GetTenXBarcodeFromRead(const io::PairedRead &read) {
        std::string delimeter = "\t";
        std::string delimeter2 = " ";
        size_t start_pos = read.first().name().find(delimeter);
        size_t delimeter_size = delimeter.length();
        if (start_pos != string::npos) {
            std::string barcode = read.first().name().substr(start_pos + delimeter_size);
            size_t end_pos = barcode.find(delimeter2);
            if (end_pos != string::npos) {
                barcode = barcode.substr(0, end_pos);
            }
            TRACE(barcode);
            return barcode;
        }
        return "";
    }

    void Process10XLibrary(debruijn_graph::conj_graph_pack &graph_pack, const lib_t& lib_10x) {
        auto mapper = MapperInstance(graph_pack);
        auto stream = io::paired_easy_reader(lib_10x, false, 0);
        std::set<std::string> used_barcodes;
        std::string current_barcode = "";
        io::PairedRead read;
        size_t counter = 0;
        std::vector<MappingPath<EdgeId>> paths;
        path_extend::PathContainer& long_reads = graph_pack.mapped_paths[lib_10x.data().lib_index];
        LongReadsCreator extractor(graph_pack);
        while (!stream->eof()) {
            *stream >> read;
            string barcode_string = GetTenXBarcodeFromRead(read);
            if (barcode_string != "") {
                if (barcode_string != current_barcode && !paths.empty()) {
                    DEBUG("Processing barcode " << current_barcode);
                    extractor.extractLongReads(paths, long_reads, current_barcode);
                    DEBUG(long_reads.size() << " long reads extracted");
                    paths.clear();
                    if (used_barcodes.count(current_barcode)) {
                        WARN("Path with " << current_barcode << " barcode was previously extracted");
                    }
                    else {
                        used_barcodes.insert(current_barcode);
                    }
                }

                const auto &path1 = mapper->MapRead(read.first());
                const auto &path2 = mapper->MapRead(read.second());
                paths.push_back(path1);
                paths.push_back(path2);
            }
            current_barcode = barcode_string;
            counter++;
            VERBOSE_POWER_T2(counter, 100, "Processed " << counter << " reads.");
        }
        DEBUG("Processing barcode " << current_barcode);
        extractor.extractLongReads(paths, long_reads, current_barcode);
        DEBUG(long_reads.size() << " long reads totally extracted");
        paths.clear();
        path_extend::ContigWriter writer(graph_pack.g, make_shared<path_extend::DefaultContigNameGenerator>());
        writer.OutputPaths(long_reads, cfg::get().output_dir + "/extracted.fasta");
    }

    void MoleculeExtractionStage::run(debruijn_graph::conj_graph_pack &graph_pack, const char *) {
        INFO("Molecule extraction for RNA stage started");
        config::dataset& dataset_info = cfg::get_writable().ds;
        if (!Has10XRNAReads(dataset_info)) {
            INFO("Read cloud libraries have not been found. Skipping molecule extraction stage.")
            return;
        }

        graph_pack.InitRRIndices();
        graph_pack.EnsureBasicMapping();

        for (auto& lib : dataset_info.reads) {
            if (lib.type() == io::LibraryType::RNA10x) {
                Process10XLibrary(graph_pack, lib);
                lib.data().single_reads_mapped = true;
            }
        }
        cfg::get_writable().use_single_reads = false;
//        io::SequencingLibrary<config::DataSetData> trusted_contigs;
//        trusted_contigs.push_back_single(cfg::get().output_dir + "extracted.fasta");
//        trusted_contigs.set_orientation(io::LibraryOrientation::Undefined);
//        trusted_contigs.set_type(io::LibraryType::TrustedContigs);
//        cfg::get_writable().ds.reads.push_back(trusted_contigs);
//        graph_pack.AddLib();
        INFO("Molecule extraction for RNA stage ended");
    }

} //debruijn_graph

#endif