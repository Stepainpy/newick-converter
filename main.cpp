#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <Windows.h>

struct clade {
	std::string name;
	int deep;
	int age;
};

// перебирает вектор с кладами
// index - ссылка, чтобы был правильный перебор
std::string create_sequence(int& index, int current_deep, std::vector<clade>& list, bool use_tl=false, int prev_age=0) {
	if (index + 1 >= list.size()) // если значение последнее
		return list.at(index).name + ",";

	std::string result = "(";
	while (index < list.size()) {
		int next_index = index + 1;

		//  если следующий существует    если со следующего начинается новый уровень
		if (next_index != list.size() && list.at(next_index).deep > current_deep)
			if (use_tl)
				result += create_sequence(next_index, list.at(next_index).deep, list, true, list.at(index).age);
			else
				result += create_sequence(next_index, list.at(next_index).deep, list);

		// создание узла ( имя, ), но с use_tl ( имя:разница_возрастов, )
		int different = (list.at(index).age < 0) ? 5 : list.at(index).age - prev_age;
		std::string age_diff = ":" + std::to_string(different);
		result += list.at(index).name + ((use_tl) ? age_diff : "") + ",";

		index = next_index;

		//  если следующий существует    если со следующего кончается нынешний уровень
		if (next_index != list.size() && list.at(next_index).deep < current_deep) break;
	}

	return result + ")";
}

// проверка что input это один из flags
bool has_flag(char* input, std::vector<std::string> flags) {
	for (const std::string& flag : flags)
		if (strcmp(input, flag.c_str()) == 0)
			return true;
	return false;
}

// помещение текста в буфер обмена
// взят с https://ru.stackoverflow.com/a/1232202
void TextToClipboard(const char* text) {
	if (OpenClipboard(0)) {
		EmptyClipboard();
		char* hBuff = (char*)GlobalAlloc(GMEM_FIXED, strlen(text) + 1);
		// strcpy(hBuff, text);
		strcpy_s(hBuff, strlen(text) + 1, text);
		SetClipboardData(CF_TEXT, hBuff);
		CloseClipboard();
	}
}

int main(int argc, char* argv[]) {
	bool out_only_leafs     = false; // вывод только листьев
	bool add_default_length = false; // добавление длины (имя:1)
	bool set_every_length   = false; // при отображении только листьев всё имеет длину
	bool to_clipboard       = false; // помещение результата сразу в буфер обмена
	bool use_timeline       = false; // использование времени появления для длины ветвей
	std::string timeline_file_name;
	
	// блок установки флагов
	if (argc < 2) {
		std::cout << "incorrect enter\n";
		return EXIT_FAILURE;
	}
	else if (has_flag(argv[1], {"-h", "--help"})) {
		std::string help_message =
			  "Usage:"
			"\nconvert [-h | --help]"
			"\nconvert <tree-file> [flags] [[-t | --timeline] <time-file>]"
			"\n"
			"\nFlags:"
			"\n-l --leafs      - views only leaf"
			"\n-a --add-length - adds a length value for named values, i.e.: (A:1,B:1),C:1"
			"\n-A --all-length - adds a length value for all values if set flag -ad, i.e.: (A:1,B:1):0.5,C:1"
			"\n-c --clipboard  - paste result in clipboard"
			"\n-t --timeline   - use timeline as branch length"
			"\n"
			"\nIf the -t flag is specified, the -a and -A flags are ignored"
			;
		std::cout << help_message;

		return EXIT_SUCCESS;
	}
	else if (argc > 2) {
		// проверка наличия флага
		for (int i = 2; i < argc; i++) {
			if (!out_only_leafs && has_flag(argv[i], { "-l", "--leafs" })) {
				out_only_leafs = true;
				continue;
			}
			
			if (!add_default_length && has_flag(argv[i], { "-a", "--add-length" })) {
				add_default_length = true;
				continue;
			}
			
			if (!set_every_length && has_flag(argv[i], { "-A", "--all-length" })) {
				add_default_length = true;
				set_every_length = true;
				continue;
			}

			if (!to_clipboard && has_flag(argv[i], { "-c", "--clipboard" })) {
				to_clipboard = true;
				continue;
			}

			if (!use_timeline && has_flag(argv[i], { "-t", "--timeline" })) {
				if (!(i + 1 < argc)) { // если файл не указан
					std::cout << "specify the file for the timeline\n";
					return EXIT_FAILURE;
				}
				use_timeline = true;
				timeline_file_name = argv[++i];
				continue;
			}

			std::cout << "unknown flag: " << argv[i] << std::endl;
			return EXIT_FAILURE;
		}

		if (use_timeline)
			add_default_length = set_every_length = false;
	}

	std::ifstream source_file, timeline_file;
	source_file.open(argv[1]);
	timeline_file.open(timeline_file_name);

	if (source_file.is_open() && (!use_timeline || timeline_file.is_open())) {
		std::vector<clade> clades = {};
		std::string rawstr;
		std::string timeline;
		const std::regex 
			cut_extra(" +\\(.*\\)"),
			counting_tabs("(    |\\t)"),
			spaces("\\s+"),
			commas(",\\)"),
			cut_nodes("\\)\\w+"),
			add_length("[a-zA-Z](?=,|\\))(?!:)"),
			all_length("\\)(?=,|\\))");

		std::string clade_age_scheme = " +(.*)\\|";
		std::string start_age_str;
		int start_age = 0;
		if (use_timeline) {
			std::getline(timeline_file, start_age_str);
			start_age = std::stoi(start_age_str);
			std::string clade_in_tl;
			while (std::getline(timeline_file, clade_in_tl))
				timeline += clade_in_tl + "|";
		} 

		std::smatch match;
		
		// проход по текстовому файлу
		while (std::getline(source_file, rawstr)) {
			// удаление значений в скобках
			std::string tabs_and_name = std::regex_replace(rawstr, cut_extra, "");

			int tabs = 0; // подсчёт отсупов
			if (std::regex_search(tabs_and_name, match, counting_tabs))
				tabs = std::distance(
					std::sregex_iterator(tabs_and_name.begin(), tabs_and_name.end(), counting_tabs),
					std::sregex_iterator());

			// удаление остального
			std::string name = std::regex_replace(tabs_and_name, spaces, "");

			int age = -1;
			if (use_timeline) {
				std::regex get_age(name + clade_age_scheme);
				if (std::regex_search(timeline, match, get_age))
					age = std::stoi(match[1].str());
			}

			clades.push_back({ name, tabs, age });
		}

		int temp = 0;
		std::string seque = std::regex_replace(create_sequence(temp, 0, clades, use_timeline, start_age), commas, ")") + ";";

		// применение флагов
		if (out_only_leafs) seque = std::regex_replace(seque, cut_nodes, ")");
		
		if (add_default_length && !use_timeline) {
			while (std::regex_search(seque, match, add_length))
				seque.insert(match.position() + 1, ":1");
			if (set_every_length)
				seque = std::regex_replace(seque, all_length, "):0.5");
		}

		std::cout << seque << std::endl; // вывод результота в консоль
		if (to_clipboard) TextToClipboard(seque.c_str()); // копирование в буфер обмена
	}
	else std::cout << "file not open\n";

	source_file.close();
	timeline_file.close();
	return EXIT_SUCCESS;
}