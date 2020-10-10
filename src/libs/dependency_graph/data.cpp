#include "data.inl"

namespace dependency_graph {

Data::Data() {
}

Data::Data(const Data& d) {
	m_data = d.m_data;
	assert(std::string(d.typeinfo().name()) == std::string(typeinfo().name()));
}

Data& Data::operator=(const Data& d) {
	// it should be possible to:
	// 1. assign values between the same types
	// 2. assign a value to a null (void) data - connecting void ports
	// 3. assign a null (void) to a value - disconnecting void ports
	assert(std::string(d.typeinfo().name()) == std::string(typeinfo().name()) || empty() || d.empty());
	m_data = d.m_data;
	return *this;
}

bool Data::operator==(const Data& d) const {
	assert(m_data != nullptr && d.m_data != nullptr);
	return m_data->isEqual(*d.m_data);
}

bool Data::operator!=(const Data& d) const {
	assert(m_data != nullptr && d.m_data != nullptr);
	return !m_data->isEqual(*d.m_data);
}

const std::type_info& Data::typeinfo() const {
	if(m_data == nullptr)
		return typeid(void);
	return m_data->typeinfo();
}

Data Data::create(const std::string& type) {
	return StaticInitialisation::create(type);
}

std::string Data::type() const {
	return dependency_graph::unmangledName(typeinfo().name());
}

std::string Data::toString() const {
	if(m_data == nullptr)
		return "(null)";
	return m_data->toString();
}

bool Data::empty() const {
	return m_data == nullptr;
}

std::ostream& operator<<(std::ostream& out, const Data& bd) {
	out << bd.toString();

	return out;
}

}  // namespace dependency_graph
