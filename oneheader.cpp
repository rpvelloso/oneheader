#include <vector>
#include <set>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <regex>
#include <experimental/filesystem>

using FileListType = std::unordered_map<std::string, size_t>;
using FilePathType = std::unordered_map<size_t, std::string>;
using Graph = std::unordered_map<size_t, std::unordered_set<size_t>>;
namespace fs = std::experimental::filesystem;

enum class VisitedType {
	Unvisited,
	Discovered,
	Visited
};

class SortIncludes {
public:
	SortIncludes(const fs::path &path) {
		sort(path);
	}

	void sort(const fs::path &path) {
		fileList.clear();
		filePath.clear();
		graph.clear();
		
		scanDir(path);
		for (auto &f:filePath)
			parseIncludes(f.first);
		topologicalSort();
	}

	std::vector<std::string> getSequence() {
		std::vector<std::string> result;
		
		for (size_t i = sequence.size(); i > 0; --i) {
			auto u = sequence[i-1];
			result.push_back(filePath[u]);
		}
		
		return result;
	}

private:
	void scanDir(const fs::path &path) {
		auto dir = fs::directory_iterator( // recursive_directory_iterator() *NOT* working under mingw
			path, 
			fs::directory_options::skip_permission_denied |
			fs::directory_options::follow_directory_symlink);
			
		for (auto &entry:dir) {
			auto filename = entry.path().filename().string();
			if (filename[0] != '.') {
				if (fs::is_directory(entry))
					scanDir(entry); // recursive_directory_iterator() *NOT* working under mingw
				else {
					if (extensions.count(entry.path().extension().string()) > 0) {
						if (fileList.find(filename) == fileList.end()) {
							size_t nodeNum = fileList.size();
							fileList[filename] = nodeNum;
							filePath[nodeNum] = entry.path().relative_path().string();
						}
					}
				}
			}
		}
	}
	
	void parseIncludes(size_t node) {
		const std::string fullpath = filePath[node]; 
		std::fstream file(fullpath);
		std::string line;
		std::regex pattern("^\\s*#\\s*include\\s+(?:<([^>]*)>|\"([^\"]*)\")\\s*");
		
		graph[node];
		
		std::getline(file, line);
		while (file) {
			auto begin = std::sregex_iterator(line.begin(), line.end(), pattern);
			auto end = std::sregex_iterator();
			
			std::for_each(begin, end, [this, node](std::smatch match) {
				std::string hdr;
				if (match.str(1) != "") {
					auto pos = match.str(1).find_last_of('/');
					if (pos == std::string::npos)
						pos = 0;
					else
						++pos;
					hdr = match.str(1).substr(pos);
				} else {
					auto pos = match.str(2).find_last_of('/');
					if (pos == std::string::npos)
						pos = 0;
					else
						++pos;
					hdr = match.str(2).substr(pos);
				}
				if (!hdr.empty()) {
					auto it = this->fileList.find(hdr);
					if (it != this->fileList.end()) {
						auto visitedNode = it->second;
						this->graph[visitedNode].insert(node);
					}
				}
			});
			
			std::getline(file, line);
		}
	}

	void DFS(size_t u) {
		visited[u] = VisitedType::Discovered;
		
		for (auto &v:graph[u]) {
			if (visited[v] == VisitedType::Discovered)
				throw std::runtime_error("Cyclic dependency encountered on " + filePath[v]);
		}
		
		for (auto &v:graph[u])
			if (visited[v] == VisitedType::Unvisited)
				DFS(v);
		
		sequence.push_back(u);
		visited[u] = VisitedType::Visited;
	}

	void topologicalSort() {
		sequence.clear();
		visited.clear();
		visited.resize(graph.size(), VisitedType::Unvisited);
		
		for (auto &u:graph)
			if (visited[u.first] == VisitedType::Unvisited)
				DFS(u.first);
	}

	FileListType fileList;
	FilePathType filePath;
	Graph graph;
	std::vector<VisitedType> visited;
	std::vector<size_t> sequence;
	static std::set<std::string> extensions;
};

std::set<std::string> SortIncludes::extensions = {
	".h", ".hpp", ".h++", ".hh"
};

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cout << "usage: " << argv[0] << " path_to_source/" << std::endl;
		return 0;
	}
	
	try {
		std::string path(argv[1]);
		
		SortIncludes sortIncludes(path);
		
		for (auto &file:sortIncludes.getSequence())
			std::cout << file << std::endl;
	} catch (std::exception &e) {
		std::cout << e.what() << std::endl;
	}
}
