#pragma once

#include <dependency_graph/metadata.h>
#include <dependency_graph/selection.h>
#include <possumwood_sdk/node_data.h>
#include <possumwood_sdk/undo_stack.h>

namespace possumwood {

struct Actions {
	static void createNode(dependency_graph::Network& current, const dependency_graph::MetadataHandle& meta,
	                       const std::string& name, const possumwood::NodeData& data,
	                       const dependency_graph::UniqueId& id = dependency_graph::UniqueId());
	static void removeNode(dependency_graph::NodeBase& node);

	static void connect(dependency_graph::Port& p1, dependency_graph::Port& p2);
	static void disconnect(dependency_graph::Port& p1, dependency_graph::Port& p2);

	template<typename T>
	static void setValue(dependency_graph::Port& p, const T& value);
	static void setValue(dependency_graph::Port& p, const dependency_graph::BaseData& value);

	static void changeMetadata(dependency_graph::NodeBase& node, const dependency_graph::MetadataHandle& handle);

	static void cut(const dependency_graph::Selection& selection);
	static void copy(const dependency_graph::Selection& selection);
	static void paste(dependency_graph::Network& current, dependency_graph::Selection& selection);
	static void remove(const dependency_graph::Selection& selection);

	static void move(const std::map<dependency_graph::NodeBase*, QPointF>& nodes);
};

template<typename T>
void Actions::setValue(dependency_graph::Port& p, const T& value) {
	const std::unique_ptr<const dependency_graph::BaseData> data(new dependency_graph::Data<T>(value));
	setValue(p, *data);
}

}
