#include "actions.h"

#include <dependency_graph/attr_map.h>
#include <dependency_graph/detail.h>
#include <dependency_graph/metadata_register.h>
#include <dependency_graph/network.h>
#include <dependency_graph/values.h>

#include <dependency_graph/node_base.inl>
#include <dependency_graph/nodes.inl>
#include <functional>
#include <set>

#include "app.h"
#include "clipboard.h"
#include "detail/connections.h"
#include "detail/metadata.h"
#include "detail/nodes.h"
#include "detail/tools.h"
#include "detail/values.h"

namespace possumwood {
namespace actions {

namespace {

void doSetBlindData(const dependency_graph::UniqueId& node, const possumwood::NodeData& blindData) {
	dependency_graph::NodeBase& n = detail::findNode(node);
	n.setBlindData(blindData);
}

}  // anonymous namespace

/////////////////////////////////////////////////////////////////////

void createNode(dependency_graph::Network& current,
                const dependency_graph::MetadataHandle& meta,
                const std::string& name,
                const possumwood::NodeData& _data,
                const dependency_graph::UniqueId& id) {
	auto action = detail::createNodeAction(current, meta, name, dependency_graph::Data(_data), id);

	possumwood::AppCore::instance().undoStack().execute(action);
}

void removeNode(dependency_graph::NodeBase& node) {
	dependency_graph::Selection selection;
	selection.addNode(node);

	auto action = detail::removeAction(selection);

	possumwood::AppCore::instance().undoStack().execute(action);
}

void connect(dependency_graph::Port& p1, dependency_graph::Port& p2) {
	auto action = detail::connectAction(p1, p2);

	possumwood::AppCore::instance().undoStack().execute(action);
}

void disconnect(dependency_graph::Port& p1, dependency_graph::Port& p2) {
	auto action = detail::disconnectAction(p1, p2);

	possumwood::AppCore::instance().undoStack().execute(action);
}

void remove(const dependency_graph::Selection& selection) {
	auto action = detail::removeAction(selection);

	possumwood::AppCore::instance().undoStack().execute(action);
}

void cut(const dependency_graph::Selection& selection) {
	// trigger the copy action first
	copy(selection);

	// and delete selection
	auto action = detail::removeAction(selection);

	possumwood::AppCore::instance().undoStack().execute(action);
}

namespace {

template <typename T>
const T& dereference(const std::reference_wrapper<T>& n) {
	return n.get();
}

template <typename T>
const T& dereference(const T& n) {
	return n;
}

possumwood::io::json writeNode(const dependency_graph::NodeBase& n) {
	possumwood::io::json j;

	j["name"] = n.name();
	j["type"] = n.metadata()->type();

	for(size_t pi = 0; pi < n.portCount(); ++pi) {
		const dependency_graph::Port& p = n.port(pi);

		// only serialize unconnected inputs
		if(p.category() == dependency_graph::Attr::kInput &&
		   ((n.hasParentNetwork() && !n.network().connections().connectedFrom(p)) || !n.hasParentNetwork()))
			if(!n.datablock().isNull(pi) && dependency_graph::io::isSaveable(n.datablock().data(pi)))
				io::toJson(j["ports"][p.name()], n.datablock().data(pi));
	}

	if(n.hasBlindData()) {
		assert(dependency_graph::io::isSaveable(n.blindData()));
		io::toJson(j["blind_data"]["value"], n.blindData());
		j["blind_data"]["type"] = n.blindDataType();
	}

	return j;
}

possumwood::io::json writeNetwork(const ::dependency_graph::Network& net,
                                  const dependency_graph::Selection& selection = dependency_graph::Selection());

/// a wrapper of node writing, with CONTAINER either a Nodes instance or a Selection
template <typename CONTAINER>
void writeNodes(possumwood::io::json& j,
                const CONTAINER& nodes,
                std::map<std::string, unsigned>& uniqueIds,
                std::map<const ::dependency_graph::NodeBase*, std::string>& nodeIds) {
	for(auto& ni : nodes) {
		const dependency_graph::NodeBase& n = dereference(ni);

		// figure out a unique name - type with a number appended
		std::string name = n.metadata()->type();
		auto slash = name.rfind('/');
		if(slash != std::string::npos)
			name = name.substr(slash + 1);
		name += "_" + std::to_string(uniqueIds[name]++);

		// and use this to save the node
		if(n.is<dependency_graph::Network>()) {
			auto tmp = writeNetwork(n.as<dependency_graph::Network>());
			for(possumwood::io::json::const_iterator i = tmp.begin(); i != tmp.end(); ++i)
				j[name][i.key()] = i.value();
		}
		else
			j[name] = writeNode(n);

		// remember the assigned ID for connection saving
		nodeIds[&n] = name;
	}
}

possumwood::io::json writeNetwork(const dependency_graph::Network& net, const dependency_graph::Selection& selection) {
	possumwood::io::json j;

	// write the base node first
	j = writeNode(net);

	// no source set - serialized into the file
	if(net.source().empty()) {
		std::map<std::string, unsigned> uniqueIds;
		std::map<const ::dependency_graph::NodeBase*, std::string> nodeIds;

		j["nodes"] = "{}"_json;
		if(selection.empty())
			writeNodes(j["nodes"], net.nodes(), uniqueIds, nodeIds);
		else
			writeNodes(j["nodes"], selection.nodes(), uniqueIds, nodeIds);

		j["connections"] = "[]"_json;
		for(auto& c : net.connections()) {
			auto itOut = nodeIds.find(&c.first.node());
			auto itIn = nodeIds.find(&c.second.node());
			if(itOut != nodeIds.end() && itIn != nodeIds.end()) {
				j["connections"].push_back("{}"_json);

				auto& connection = j["connections"].back();

				connection["out_node"] = itOut->second;
				connection["out_port"] = c.first.name();
				connection["in_node"] = itIn->second;
				connection["in_port"] = c.second.name();
			}
		}
	}

	// source set - write just the filename to load from, not the content
	else {
		j["source"] = net.source().string();
	}

	return j;
}

}  // namespace

possumwood::io::json toJson(const dependency_graph::Selection& selection) {
	dependency_graph::Network* net = &possumwood::AppCore::instance().graph();
	if(!selection.empty() && selection.nodes().begin()->get().hasParentNetwork())
		net = &selection.nodes().begin()->get().network();

	// convert the selection to JSON string
	possumwood::io::json json = writeNetwork(*net, selection);

	return json;
}

void copy(const dependency_graph::Selection& selection) {
	auto json = toJson(selection);

	std::stringstream ss;
	ss << std::setw(4) << json;

	// and put it to the clipboard
	Clipboard::instance().setClipboardContent(ss.str().c_str());
}

namespace {

enum PasteFlags { kNone = 0, kRoot = 1 };

dependency_graph::State pasteNetwork(possumwood::UndoStack::Action& action,
                                     const dependency_graph::UniqueId& targetIndex,
                                     const possumwood::io::json& _source,
                                     PasteFlags flags,
                                     std::set<dependency_graph::UniqueId>* ids = nullptr) {
	dependency_graph::State state;
	possumwood::io::json tmp;

	// pasted network should actually be loaded from a file
	const possumwood::io::json* source = nullptr;
	if(_source.find("source") != _source.end()) {
		auto stream = AppCore::instance().filesystem().read(
		    possumwood::Filepath::fromString(_source["source"].get<std::string>()));
		(*stream) >> tmp;

		source = &tmp;
	}
	// should be loaded from the json directly
	else {
		source = &_source;
	}

	// indices of newly loaded nodes
	std::map<std::string, dependency_graph::UniqueId> nodeIds;

	// add all the nodes to the parent network
	//  - each node has a unique ID (unique between all graphs), store that
	if(source->find("nodes") != source->end()) {
		for(possumwood::io::json::const_iterator ni = (*source)["nodes"].begin(); ni != (*source)["nodes"].end();
		    ++ni) {
			const possumwood::io::json& n = ni.value();

			// extract the blind data via factory mechanism
			dependency_graph::Data blindData;
			if(n.find("blind_data") != n.end() && !n["blind_data"].is_null()) {
				blindData = dependency_graph::Data::create(n["blind_data"]["type"].get<std::string>());
				assert(!blindData.empty());
				assert(dependency_graph::io::isSaveable(blindData));
				io::fromJson(n["blind_data"]["value"], blindData);
			}

			// find the metadata instance
			auto metaIt = dependency_graph::MetadataRegister::singleton().find(n["type"].get<std::string>());
			if(metaIt == dependency_graph::MetadataRegister::singleton().end())
				state.addError(
				    "Unregistered node type '" + n["type"].get<std::string>() +
				    "' found in the setup - maybe a missing plugin? Skipping the creation of unregistered node type.");

			else {
				// generate a new unique index for the node
				assert(nodeIds.find(ni.key()) == nodeIds.end());
				const dependency_graph::UniqueId nodeId;
				nodeIds.insert(std::make_pair(ni.key(), nodeId));

				if(ids)
					ids->insert(nodeId);

				// add the action to create the node itself
				action.append(
				    detail::createNodeAction(targetIndex, *metaIt, n["name"].get<std::string>(), blindData, nodeId));

				// recurse to add nested networks
				//   -> this will also construct the internals of the network, and instantiate
				//      its inputs and outputs
				if(n["type"] == "network")
					state.append(pasteNetwork(action, nodeId, n, kNone));

				// networks process ports during pasteNetwork() call; nodes don't have an explicit step, so let's do
				// that here
				else {
					// and another action to set all port values based on the json content
					//   -> as the node doesn't exist yet, we can't interpret the types
					if(n.find("ports") != n.end())
						for(possumwood::io::json::const_iterator pi = n["ports"].begin(); pi != n["ports"].end(); ++pi)
							action.append(detail::setValueAction(nodeId, pi.key(), pi.value()));
				}
			}
		}
	}

	// add all connections, based on "unique" IDs
	if(source->find("connections") != source->end()) {
		for(auto& c : (*source)["connections"]) {
			auto id1 = nodeIds.find(c["out_node"].get<std::string>());
			auto port1 = c["out_port"].get<std::string>();

			auto id2 = nodeIds.find(c["in_node"].get<std::string>());
			auto port2 = c["in_port"].get<std::string>();

			if(id1 == nodeIds.end())
				state.addError("Connection from a non-existing node '" + c["out_node"].get<std::string>() +
				               "' cannot be added!");

			if(id2 == nodeIds.end())
				state.addError("Connection to a non-existing node '" + c["in_node"].get<std::string>() +
				               "' cannot be added!");

			if(id1 != nodeIds.end() && id2 != nodeIds.end())
				action.append(detail::connectAction(id1->second, port1, id2->second, port2));
		}
	}

	// and add the "source" if any, with a compressed filepath
	if(_source.find("source") != _source.end() && !(flags & kRoot)) {
		action.append(detail::setSourceAction(
		    targetIndex, possumwood::Filepath::fromString(_source["source"].get<std::string>()).toString()));
	}

	// and another action to set all port values based on the json content
	//   -> as the network doesn't exist yet, we can't interpret the types
	if(source->find("ports") != source->end())
		for(possumwood::io::json::const_iterator pi = (*source)["ports"].begin(); pi != (*source)["ports"].end(); ++pi)
			action.append(detail::setValueAction(targetIndex, pi.key(), pi.value()));

	// finally, overwrite any of the ports in the original source with new values that might be present in the
	// referencing file
	if(_source.find("ports") != _source.end())
		for(possumwood::io::json::const_iterator pi = _source["ports"].begin(); pi != _source["ports"].end(); ++pi)
			action.append(detail::setValueAction(targetIndex, pi.key(), pi.value()));

	return state;
}
}  // namespace

void paste(dependency_graph::Network& current, dependency_graph::Selection& selection) {
	paste(current, selection, Clipboard::instance().clipboardContent());
}

dependency_graph::State paste(dependency_graph::Network& current,
                              dependency_graph::Selection& selection,
                              const std::string& content,
                              bool haltOnError) {
	dependency_graph::State state;

	try {
		// convert the clipboard content to a json object
		auto json = possumwood::io::json::parse(content);

		// and pass it to the paste() implementation
		state = fromJson(current, selection, json, haltOnError);
	}
	catch(std::exception& e) {
// do nothing
#ifndef NDEBUG
		std::cout << e.what() << std::endl;
#endif
	}

	return state;
}

dependency_graph::State fromJson(dependency_graph::Network& current,
                                 dependency_graph::Selection& selection,
                                 const possumwood::io::json& json,
                                 bool haltOnError) {
	dependency_graph::State state;

	possumwood::UndoStack::Action action;

	std::set<dependency_graph::UniqueId> pastedNodeIds;

	// paste the network extracted from the JSON
	state.append(pasteNetwork(action, current.index(), json, kRoot, &pastedNodeIds));

	// execute the action (will actually make the nodes and connections)
	state.append(possumwood::AppCore::instance().undoStack().execute(action, haltOnError));

	// and make the selection based on added nodes
	for(auto& n : pastedNodeIds)
		selection.addNode(detail::findNode(n));

	return state;
}

void move(const std::map<dependency_graph::NodeBase*, possumwood::NodeData::Point>& nodes) {
	possumwood::UndoStack::Action action;

	for(auto& n : nodes) {
		const possumwood::NodeData originalData = n.first->blindData<possumwood::NodeData>();

		if(originalData.position() != n.second) {
			possumwood::NodeData data = originalData;
			data.setPosition(n.second);

			action.addCommand("Move " + n.first->name(), std::bind(&doSetBlindData, n.first->index(), data),
			                  std::bind(&doSetBlindData, n.first->index(), n.first->blindData<possumwood::NodeData>()));
		}
	}

	possumwood::AppCore::instance().undoStack().execute(action);
}

void changeMetadata(dependency_graph::NodeBase& node, const dependency_graph::MetadataHandle& handle) {
	possumwood::UndoStack::Action action = detail::changeMetadataAction(node, handle);

	possumwood::AppCore::instance().undoStack().execute(action);
}

void setValue(dependency_graph::Port& port, const dependency_graph::Data& value) {
	possumwood::UndoStack::Action action = detail::setValueAction(port, value);

	AppCore::instance().undoStack().execute(action);
}

void renameNode(dependency_graph::NodeBase& node, const std::string& name) {
	possumwood::UndoStack::Action action = detail::renameNodeAction(node.index(), name);

	AppCore::instance().undoStack().execute(action);
}

dependency_graph::State importNetwork(dependency_graph::Network& current,
                                      dependency_graph::Selection& selection,
                                      const possumwood::Filepath& filepath,
                                      const std::string& name,
                                      const dependency_graph::Data& blindData) {
	dependency_graph::State state;

	possumwood::UndoStack::Action action;

	possumwood::io::json tmp;
	tmp["source"] = filepath.toString();

	const dependency_graph::UniqueId networkId;

	auto metaIt = dependency_graph::MetadataRegister::singleton().find("network");

	action.append(detail::createNodeAction(current.index(), *metaIt, name, blindData, networkId));

	// paste the network extracted from the JSON
	std::set<dependency_graph::UniqueId> pastedNodeIds;
	state.append(pasteNetwork(action, networkId, tmp, kNone, &pastedNodeIds));

	// execute the action (will actually make the nodes and connections)
	state.append(possumwood::AppCore::instance().undoStack().execute(action, false));

	// and make the selection based on added nodes
	selection.addNode(detail::findNode(networkId));

	return state;
}

}  // namespace actions
}  // namespace possumwood
