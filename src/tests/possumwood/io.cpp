#include "io.h"

#include <actions/io.h>
#include <actions/node_data.h>
#include <dependency_graph/rtti.h>

#include <dependency_graph/data.inl>

namespace dependency_graph {
namespace io {

struct SaveableRegistration {
	SaveableRegistration() {
		setIsSaveableCallback([](const Data&) { return true; });
	}

	~SaveableRegistration() {
		setIsSaveableCallback(std::function<bool(const Data& data)>());
	}
};

static SaveableRegistration s_saveableRegistration;

}  // namespace io
}  // namespace dependency_graph

namespace possumwood {
namespace io {

void fromJson(const nlohmann::json& j, dependency_graph::Data& data) {
	if(data.type() == "float") {
		data.set<float>(j.get<float>());
	}
	else if(data.type() == dependency_graph::unmangledTypeId<std::string>()) {
		data.set<std::string>(j.get<std::string>());
	}
	else if(data.type() == dependency_graph::unmangledTypeId<possumwood::NodeData>()) {
		data.set<possumwood::NodeData>(possumwood::NodeData());
	}
	else
		assert(false);
}

void toJson(nlohmann::json& j, const dependency_graph::Data& data) {
	if(data.type() == "float") {
		j = data.get<float>();
	}
	else if(data.type() == dependency_graph::unmangledTypeId<std::string>()) {
		j = data.get<std::string>();
	}
	else if(data.type() == dependency_graph::unmangledTypeId<possumwood::NodeData>()) {
		j = "test blind data";
	}
	else
		assert(false);
}

bool isSaveable(const dependency_graph::Data& data) {
	return true;
}

}  // namespace io
}  // namespace possumwood
