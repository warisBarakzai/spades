#include "dataset_processor.hpp"
#include <iostream>
#include "io/ireader.hpp"
#include "io/converting_reader_wrapper.hpp"
#include "io/file_reader.hpp"
#include "path_helper.hpp"
#include <omp.h>
using namespace std;
namespace corrector{


void DatasetProcessor::SplitGenome(const string &genome, const string &genome_splitted_dir, ContigInfoMap &all_contigs){
    io::FileReadStream contig_stream(genome);
    io::SingleRead ctg;
    while (! contig_stream.eof()) {
    	contig_stream >> ctg;
    	string contig_name = ctg.name();
    	//INFO(contig_name);
    	string full_path = genome_splitted_dir + "/" + contig_name+ ".fasta";
    	string out_full_path = 	 full_path.substr(0, full_path.length() - 5) + "ref.fasta";
    	string sam_filename =  full_path.substr(0, full_path.length() - 5) + "pair.sam";
    	//INFO("full_path:" << full_path);
    	all_contigs[contig_name].input_contig_filename = full_path;
    	all_contigs[contig_name].output_contig_filename = out_full_path;
    	all_contigs[contig_name].sam_filename = sam_filename;
    	all_contigs[contig_name].contig_length = ctg.sequence().str().length();
    	io::osequencestream oss(full_path);
    	oss << ctg;
    }
}
//returns names
void DatasetProcessor::GetAlignedContigs(string &read, set<string> &contigs) {
	vector<string> arr = split(read, '\t');
	if (arr.size() > 5) {
		if (arr[2] != "*" && stoi(arr[4]) > 0) {
//TODO:: here can be multuple aligned parsing if neeeded;
			contigs.insert(arr[2]);
		}
	}

}


void DatasetProcessor::SplitSingleLibrary(){

}

void DatasetProcessor::PrepareWriters(ContigInfoMap &all_contigs){
//TODO::place for buffered writers;
	for (auto ac :all_contigs){

		all_writers[ac.first] = new ofstream(ac.second.sam_filename.c_str());
	}
}

void DatasetProcessor::CloseWriters(ContigInfoMap &all_contigs){
//TODO::place for buffered writers;
	for (auto ac :all_contigs){
		all_writers[ac.first]->close();
	}
}


void DatasetProcessor::OutputRead(string &read, string &contig_name) {
	//TODO:: buffered_writer
	*all_writers[contig_name] << read ;
	*all_writers[contig_name] <<  '\n';
}

void DatasetProcessor::SplitPairedLibrary(string &all_reads_filename, ContigInfoMap &all_contigs){
	ifstream fs(all_reads_filename);
	while (! fs.eof()){
		set<string> contigs;
		string r1;
		string r2;
		getline(fs, r1);
		if (r1[0] =='@') continue;
		getline(fs, r2);

		GetAlignedContigs(r1, contigs);
		GetAlignedContigs(r2, contigs);

		for (string contig: contigs) {
			VERIFY_MSG(all_contigs.find(contig) != all_contigs.end(), "wrong contig name in SAM file header: " + contig);
			OutputRead(r1, contig);
			OutputRead(r2, contig);
		}
	}
}

void DatasetProcessor::SplitHeaders(string &all_reads_filename, ContigInfoMap &all_contigs) {
	ifstream fs(all_reads_filename);
	while (! fs.eof()){
		string r;
		getline(fs, r);
		if (r.length() == 0 || r[0] != '@') {
			break;
		}
		vector<string > arr = split(r, '\t');
		if (arr[0] == "@SQ") {
			VERIFY_MSG(arr.size() > 1, "Invalid .sam header");
			string contig_name = arr[1].substr(3, arr[1].length() - 3) ;
			//INFO(contig_name);
			VERIFY_MSG(all_writers.find(contig_name) != all_writers.end(), "wrong contig name in SAM file header");
			OutputRead(r, contig_name);
		}
	}
}


void DatasetProcessor::ProcessLibrary(string &sam_file){
	INFO("Splitting genome");
	SplitGenome(genome_file, work_dir, all_contigs);
	PrepareWriters(all_contigs);
	SplitHeaders(sam_file, all_contigs);
	INFO("Splitting paired library");
	SplitPairedLibrary(sam_file, all_contigs);
	CloseWriters(all_contigs);
	INFO("Processing contigs");
	vector<pair<size_t, string> > ordered_contigs;
	ContigInfoMap all_contigs_copy;
	for (auto ac: all_contigs) {
		ordered_contigs.push_back(make_pair(ac.second.contig_length, ac.first));
		all_contigs_copy.insert(ac);
	}
	size_t cont_num = ordered_contigs.size();
	sort(ordered_contigs.begin(), ordered_contigs.end());
	reverse(ordered_contigs.begin(), ordered_contigs.end());

# pragma omp parallel for shared(all_contigs_copy, ordered_contigs) num_threads(nthreads)
	for (size_t i = 0; i < cont_num; i++ ) {

		if ( ordered_contigs[i].first > 20000)
			INFO("processing contig" << ordered_contigs[i].second << " in thread number " << omp_get_thread_num());
		ContigProcessor pc(all_contigs_copy[ordered_contigs[i].second].sam_filename, all_contigs_copy[ordered_contigs[i].second].input_contig_filename);
		pc.process_sam_file();
	}

	INFO("Gluing processed contigs");
	GlueSplittedContigs(output_contig_file, all_contigs);
}

void DatasetProcessor::GlueSplittedContigs(string &out_contigs_filename, ContigInfoMap &all_contigs){

	ofstream of_c(out_contigs_filename, std::ios_base::binary);
	for (auto ac : all_contigs) {
		string a = ac.second.output_contig_filename;
		ifstream a_f(a, std::ios_base::binary);
		of_c << a_f.rdbuf();
	}
}

void DatasetProcessor::ProcessSplittedLibrary(){

}


};
