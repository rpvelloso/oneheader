#include <vector>
#include <set>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <regex>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

class DetectIncludeFiles {
public:
	DetectIncludeFiles(const std::string &filename) {
		std::fstream file(filename);
		std::stringstream ss;

		ss << file.rdbuf();
		ss.str(removeBlockComments(ss.str()));
		parse(ss);
	}

	const std::vector<std::string> &getIncludes() {
		return includes;
	}
private:
	std::vector<std::string> includes;
	static std::set<char> ignore;

	std::string removeBlockComments(const std::string &inp) {
		int transition[4][2] = {
			{0, 1},
			{0, 2},
			{2, 3},
			{2, 0}
		};

		int state = 0;
		int type = -1;

		std::string result;

		for (auto ch:inp) {
			switch (state) {
			case 0:
				if (ch == '/')
					type = 1;
				else
					type = 0;

				if (type == 0)
					result.push_back(ch);
				break;
			case 1:
				if (ch == '*')
					type = 1;
				else
					type = 0;

				if (type == 0) {
					result.push_back('/');
					result.push_back(ch);
				}
				break;
			case 2:
				if (ch == '*')
					type = 1;
				else
					type = 0;
				break;
			case 3:
				if (ch == '/')
					type = 1;
				else
					type = 0;
				break;
			default:
				throw std::runtime_error("invalid state - block comment");
			}
			state = transition[state][type];
		}
		return result;
	}

	std::string parseInclude(const std::string line) { // parse #include
		int transitions[2][2] = {
			{0, 1},
			{1,-1}
		};

		int state = 0;
		int type = -1;
		char close = '>';

		std::string clause, header;

		auto it = line.begin();
		while (it != line.end() && ignore.count(*it) > 0)
			++it;

		if (it != line.end() && *it == '#') {
			for (auto c = it; c != line.end(); ++c) {
				auto ch = *c;
				switch(state) {
				case 0:
					if (ch == '<') {
						type = 1;
						close = '>';
					} else if (ch == '\"') {
						type = 1;
						close = ch;
					} else
						type = 0;

					if (type == 0 && ignore.count(ch) == 0)
						clause.push_back(ch);
					break;
				case 1:
					if (ch == close) {
						if (clause == "#include")
							return header;
					} else
						type = 0;

					if (type == 0 && ignore.count(ch) == 0)
						header.push_back(ch);
					break;
				default:
					throw std::runtime_error("invalid state - include");
				}
				state = transitions[state][type];
			}
		}
		return "";
	}

	void parse(std::istream &inp) { // removes line comments and parses include directives
		std::string line;

		std::getline(inp, line);
		while (inp) {
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			auto pos = line.find("//");
			if (pos != std::string::npos)
				line = line.substr(0, pos);

			auto header = parseInclude(line);
			if (header != "")
				includes.push_back(header);

			std::getline(inp, line);
		}
	}
};

std::set<char> DetectIncludeFiles::ignore = {' ', '\t'};

using FileListType = std::unordered_map<std::string, size_t>;
using FilePathType = std::unordered_map<size_t, std::string>;
using Graph = std::unordered_map<size_t, std::unordered_set<size_t>>;

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

		DetectIncludeFiles detectIncludes(fullpath);
		
		graph[node];
		
		for (auto &inc:detectIncludes.getIncludes()) {
			fs::path hdr(inc);
			auto it = this->fileList.find(hdr.filename().string());
			if (it != this->fileList.end()) {
				auto visitedNode = it->second;
				this->graph[visitedNode].insert(node);
			}
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
