//
// keyevt
//
// A simple key event daemon for Linux
//
// (C) 2015 Alexander Holler
//
// Build it with
//
//   g++ -s -O3 -Wall -pedantic -std=gnu++11 -pthread -o keyevt keyevt.cpp
//
#include <iostream>
#include <system_error>
#include <thread>
#include <chrono>
#include <ctime>
#include <cstring> // memset
#include <fstream>
#include <map>

#include <fcntl.h> // open
#include <unistd.h> // close
#include <sys/ioctl.h>
#include <linux/input.h>

class Input {
public:
	Input()
		: file_(0)
	{}
	~Input() {
		close();
	}
	void open(const std::string &input) {
		file_ = ::open(input.c_str(), O_RDONLY | O_CLOEXEC);
		if (file_ <= 0) {
			file_ = 0;
			throw std::system_error(std::error_code(errno, std::system_category()),
				"Can't open input device");
		}
		char buf[80];
		std::memset(buf, 0, sizeof(buf));
		if (::ioctl(file_, EVIOCGNAME(sizeof(buf)), buf) < 0)
			throw std::system_error(std::error_code(errno, std::system_category()),
				"ioctl error");
		buf[sizeof(buf)-1] = 0;
		name_ = buf;
	}
	void close(void) {
		if (file_)
			::close(file_);
		file_ = 0;
	}
	bool try_get_event(input_event& evt) {
		int rc = ::read(file_, &evt, sizeof(evt));
		if (rc < 0) {
			if (errno == EWOULDBLOCK)
				return false;
			throw std::system_error(std::error_code(errno,
				std::system_category()), "read error");
		}
		return (rc == sizeof(evt));
	}
	std::string get_name(void) const {
		return name_;
	}
private:
	std::string name_;
	int file_;
};

// Removes leading and trailing spaces and tabs
static void strip(std::string& s)
{
	auto pos = s.find_first_not_of(" \t");
	if (pos != std::string::npos)
		s = s.substr(pos);
	pos = s.find_last_not_of(" \t");
	if (pos != std::string::npos)
		s = s.substr(0, pos+1);
}

static std::string get_and_remove_first_word(std::string& s)
{
	auto pos = s.find_first_not_of(" \t");
	if (pos == std::string::npos) {
		s.clear();
		return s;
	}
	if (pos)
		s.erase(0, pos);
	pos = s.find_first_of(" \t");
	if (pos == std::string::npos) {
		std::string word(s);
		s.clear();
		return word;
	}
	std::string word(s.substr(0, pos));
	s.erase(0, pos);
	return word;
}

struct Event {
	unsigned ratelimit_seconds;
	bool on_press;
	bool pressed;
	std::chrono::time_point<std::chrono::steady_clock> time_last_executed;
	std::string exec;
};

typedef std::map<unsigned, Event> Map_key_event;
static Map_key_event map_key_event;

static void parse_config(const std::string& filename)
{
	std::ifstream in(filename.c_str());
	std::string s;
	while (std::getline(in, s)) {
		strip(s);
		if (s.empty())
			continue;
		if (s[0] == '#')
			continue;
		std::string first_word = get_and_remove_first_word(s);

		unsigned key = std::strtoul(first_word.c_str(), nullptr, 10);
		Event evt;
		first_word = get_and_remove_first_word(s);
		evt.ratelimit_seconds = std::strtoul(first_word.c_str(), nullptr, 10);
		first_word = get_and_remove_first_word(s);
		evt.on_press = std::strtoul(first_word.c_str(), nullptr, 10);
		auto start = s.find_first_not_of(" \t");
		if (start == std::string::npos)
			continue;
		evt.exec = s.substr(start);
		evt.pressed = false;
		map_key_event.insert(std::make_pair(key, std::move(evt)));
	}
}

static void exec(const std::string &str)
{
	std::thread t([](const std::string& s){
		int unused __attribute__((unused)) = std::system(s.c_str());
	}, str);
	t.detach();
}

int main(int argc, char* argv[])
{
	std::cout << "\nkeyevt V1.0\n";
	std::cout << "\n(C) 2015 Alexander Holler\n\n";

	if ((argc != 3) ) {
		std::cerr << "Usage: keyevt inputdevice config\n\n";
		return 1;
	}

	parse_config(argv[2]);

	if (map_key_event.empty()) {
		std::cerr << "No key events!\n";
		return 2;
	}

	Input input;
	input.open(argv[1]);

	std::cout << "Input device '" << argv[1] << "' (" << input.get_name() << ")\n";

	for (const auto& k : map_key_event)
		std::cout << "keycode " << k.first <<
			" ratelimit " << k.second.ratelimit_seconds <<
			" on_press " << k.second.on_press <<
			" exec '" << k.second.exec << "'\n";

	input_event evt;

	for(;;) {
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		if (!input.try_get_event(evt))
			continue;
		if (evt.type != EV_KEY)
			continue;
		auto k = map_key_event.find(evt.code);
		if (k == map_key_event.cend())
			continue;
		if (evt.value && k->second.pressed)
			continue;
		if (!evt.value && !k->second.pressed)
			continue;
		k->second.pressed = evt.value;
		if (k->second.pressed != k->second.on_press)
			continue;
		if (std::chrono::steady_clock::now() - k->second.time_last_executed <= std::chrono::seconds(k->second.ratelimit_seconds))
			continue; // rate limited
		k->second.time_last_executed = std::chrono::steady_clock::now();
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		std::time_t now_c = std::chrono::system_clock::to_time_t(now);
		std::string t(std::ctime(&now_c));
		t.erase(t.find_last_not_of(" \n")+1);
		std::cout << t << " key code " << k->first <<
			(k->second.pressed ? " pressed" : " released") <<
			", executing '" << k->second.exec << "'\n";
		exec(k->second.exec);
	}

	return 0;
}
