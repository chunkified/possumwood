#include "filenames.h"

#include "possumwood_sdk/app.h"

namespace possumwood {

Filenames::Filenames(std::initializer_list<std::string> extensions) : m_extensions(extensions) {
}

Filenames::Filenames(const Filenames& fn) : m_filenames(fn.m_filenames), m_extensions(fn.m_extensions) {
}

const std::vector<possumwood::Filepath>& Filenames::filenames() const {
	return m_filenames;
}

bool Filenames::empty() const {
	return m_filenames.empty();
}

void Filenames::addFilename(const possumwood::Filepath& filename) {
	m_filenames.push_back(filename);
}

void Filenames::clear() {
	m_filenames.clear();
}

const std::set<std::string>& Filenames::extensions() const {
	return m_extensions;
}

Filenames& Filenames::operator=(const Filenames& fn) {
	// only assign a value if the m_extension array is empty
	// -> allows to keep the extensions list while allowing to change
	//    the filename value in the UI / serialization
	if(m_extensions.empty())
		m_extensions = fn.m_extensions;

	m_filenames = fn.m_filenames;

	return *this;
}

bool Filenames::operator==(const Filenames& fn) const {
	return m_filenames == fn.m_filenames && m_extensions == fn.m_extensions;
}

bool Filenames::operator!=(const Filenames& fn) const {
	return m_filenames != fn.m_filenames || m_extensions != fn.m_extensions;
}

std::ostream& operator<<(std::ostream& out, const Filenames& f) {
	out << f.filenames().size() << " filename(s)" << std::endl;

	for(auto& fi : f.filenames())
		out << "  - " << fi.toString() << std::endl;

	return out;
}

/////////////////

namespace {

void toJson(::nlohmann::json& json, const Filenames& value) {
	if(value.empty())
		json = "";
	else {
		auto fn = value.filenames();
		if(fn.size() == 1)
			json = fn[0].toString();
		else {
			for(auto& fff : fn)
				json.push_back(fff.toString());
		}
	}
}

void fromJson(const ::nlohmann::json& json, Filenames& value) {
	value.clear();

	if(!json.empty()) {
		if(json.is_string())
			value.addFilename(possumwood::Filepath::fromString(json.get<std::string>()));
		else {
			assert(json.is_array());
			for(std::size_t i = 0; i < json.size(); ++i)
				value.addFilename(possumwood::Filepath::fromString(json[i].get<std::string>()));
		}
	}
}

}  // namespace

IO<Filenames> Traits<Filenames>::io(&toJson, &fromJson);

}  // namespace possumwood
