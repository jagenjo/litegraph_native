#include "litegraph.h"
#include <cassert>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include "libs/cJSON.h"

bool LiteGraph::verbose = false;
std::map<std::string, LiteGraph::LGraphNode*> LiteGraph::node_types;

LiteGraph::vec3 LiteGraph::hex2rgb(std::string hex)
{
    unsigned int r, g, b;
    if (hex.length() == 4)//in case has the form #fff instead of #ffffff
    {
        char x, y, z;
        char buffer[8];
        sscanf_s(hex.c_str(), "#%c%c%c", &x, &y, &z);
        snprintf(buffer, sizeof buffer, "#%c%c%c%c%c%c", x, x, y, y, z, z);
        hex = buffer;
    }
    sscanf_s(hex.c_str(), "#%2x%2x%2x", &r, &g, &b);

    vec3 color = { r,g,b };
    return color;
}

std::string LiteGraph::rgb2hex(vec3 color)
{
    char hexcol[8];
    snprintf(hexcol, sizeof hexcol, "#%02x%02x%02x", int(color.x), int(color.y), int(color.z) );
    std::string result = hexcol;
    return result;
}

LiteGraph::LData::LData() {
	type = DataType::NONE;
	bytes = 0;
	custom_data = NULL;
}

LiteGraph::LData::~LData()
{
	clear();
}

void LiteGraph::LData::clear()
{
	if (custom_data)
	{
		if (bytes)
		{
			if (type == DataType::ARRAY) //array is the only case that contains pointers
			{
				LData* d = (LData*)custom_data;
				int num = bytes / sizeof(LData);
				for (int i = 0; i < num; ++i)
					d[i].clear();
			}
			//LEvent no need to control, it doesnt contains pointers
			delete[] custom_data;
			bytes = 0;
		}
		custom_data = NULL;
	}
}

void LiteGraph::LData::setType(DataType type)
{
	if (type == this->type)
		return;
	this->type = type;
	int size = 0;
	switch (type)
	{
		case DataType::MAT3: size = sizeof(mat3); break;
		case DataType::MAT4: size = sizeof(mat4); break;
		case DataType::EVENT: size = sizeof(LEvent); break;
		default: size = 0;
	}
	if (bytes != size)
		clear();
	if (size)
	{
		custom_data = (void*)new uint8_t[size];
		bytes = size;
		memset(custom_data, 0, bytes);
	}
}

void LiteGraph::LData::assign(mat3 v)
{
	if (type != DataType::MAT3)
		setType(DataType::MAT3);
	memcpy(custom_data, &v, bytes);
}

void LiteGraph::LData::assign(mat4 v)
{
	if (type != DataType::MAT4)
		setType(DataType::MAT4);
	memcpy(custom_data, &v, bytes);
}

void LiteGraph::LData::assign(const LEvent& v)
{
	if (type != DataType::EVENT)
		setType(DataType::EVENT);
	LEvent* e = (LEvent*)custom_data;
	*e = v;
}

void LiteGraph::LData::assign(const char* str)
{
	int l = strlen(str) + 1; //plus one for the '\0' character
	if (type != DataType::STRING)
		setType(DataType::STRING); //clears
	if (l != bytes)
	{
		clear();
		custom_data = new char[l];
		bytes = l;
	}
	strcpy_s((char*)custom_data, l, str);
}

void LiteGraph::LData::assign(const std::string& str)
{
	int l = str.size() + 1; //plus one for the '\0' character
	if (type != DataType::STRING)
		setType(DataType::STRING); //clears
	if (l != bytes)
	{
		clear();
		custom_data = new char[l];
		bytes = l;
	}
	strcpy_s( (char*)custom_data, l, str.c_str());
}

void LiteGraph::LData::assign(void* pointer, int size)
{
	if (type != DataType::OBJECT)
		setType(DataType::OBJECT);//clear

	if(bytes != size)
	{
		clear();
		custom_data = (void*)new uint8_t[size];
		bytes = size;
	}

	if (pointer)
		memcpy(custom_data, pointer, bytes);
	else //allows to send NULL to just allocate space
		memset(custom_data, 0, bytes); //set to zero
}

template<class T> 
void LiteGraph::LData::assignObject(const T& obj)
{
	assign((void*)&obj,sizeof(T) );
}

void LiteGraph::LData::assign(const std::vector<LData*>& v)
{
	//arrays are converted to an array of data, not pointers of data
	setType(DataType::ARRAY);
	clear();
	LData* d = new LData[v.size()];
	for (unsigned int i = 0; i < v.size(); ++i)
		d[i] = *v[i];
	custom_data = (void*)d;
}

std::vector<float> LiteGraph::LData::getArrayOfFloat()
{
	std::vector<float> v;
	v.resize(bytes / sizeof(float)); 
	memcpy(&v[0], custom_data, bytes);
	return v;
}

std::vector<LiteGraph::LData*> LiteGraph::LData::getArray()
{
	LData* d = (LData*)custom_data;
	std::vector<LData*> r;
	unsigned int num = bytes / sizeof(LData*);
	r.resize(num);
	for (unsigned int i = 0; i < num; ++i)
		r[i] = d + i;
	return r;
}

std::string LiteGraph::LData::getString()
{
	if (bytes == 0 || type != DataType::STRING) //careful if not null terminates string
		return NULL;
	std::string result;
	result.resize(bytes);
	strcpy_s(&result[0], bytes, (char*)custom_data );
	return result;
}

LiteGraph::LEvent LiteGraph::LData::getEvent()
{
	LEvent e;
	if (type == DataType::EVENT && bytes) //careful if not null terminates string
		memcpy(&e, custom_data, sizeof(LEvent));
	return e;
}

void* const LiteGraph::LData::getPointer()
{
	if (type != DataType::POINTER)
		return NULL;
	return pointer;
}

void* LiteGraph::LData::getObject()
{
	if (type != DataType::OBJECT)
		return NULL;
	return custom_data;
}

void LiteGraph::LData::operator = (const LData& v)
{
	clear();
	memcpy(this, &v, sizeof(LData)); //clone content
	if (bytes) //clone allocated bytes
	{
		void* newp = new uint8_t[bytes];
		memcpy(newp, v.custom_data, bytes);
		custom_data = newp;
		//no need to control DataType::ARRAY, as they are not pointers
	}
}


/*
template<class T>
void LiteGraph::LData::setObject(T obj)
{
	int size = sizeof(T);
	custom_data = (void*)obj;
}
*/

LiteGraph::LSlot::~LSlot()
{
	if (!node || !node->graph)
		return;

	if (link) //input
	{
		LGraphNode* origin_node = node->graph->getNodeById( link->origin_id );
		if (origin_node->outputs.size() > link->origin_slot)
		{
			LSlot* origin_slot = origin_node->outputs[link->origin_slot];
			//remove link
			//TODO
		}
	}

	for (int i = 0; i < links.size(); ++i) //output
	{
	}
}

LiteGraph::LData* LiteGraph::LSlot::getOriginData()
{
	//get link
	if (!link || !node || !node->graph ) //!links.size()
		return NULL;
	LGraphNode* origin_node = node->graph->getNodeById( link->origin_id );
	if (!origin_node)
		return NULL;
	LSlot* output_slot = origin_node->outputs[link->origin_slot];
	if (!output_slot)
		return NULL;
	return output_slot->data;
}


LiteGraph::LGraphNode::LGraphNode()
{
	graph = NULL;
	id = -1;
	flags = 0;
	custom_data = NULL;
}

LiteGraph::LGraphNode::~LGraphNode()
{
	removeSlots();
}

LiteGraph::LSlot* LiteGraph::LGraphNode::addInput(const char* name, LiteGraph::DataType type)
{
	LSlot* slot = new LiteGraph::LSlot(this, name, type);
	inputs.push_back(slot);
	return slot;
}

LiteGraph::LSlot* LiteGraph::LGraphNode::addOutput(const char* name, LiteGraph::DataType type)
{
	LSlot* slot = new LiteGraph::LSlot(this, name, type);
	outputs.push_back(slot);

	slot->data = new LData();
	//slot->data->type = type;
	return slot;
}

LiteGraph::LSlot* LiteGraph::LGraphNode::getInputSlot(int i)
{
	if (i >= (int)inputs.size())
		return NULL;
	return inputs[i];
}

LiteGraph::LSlot* LiteGraph::LGraphNode::getOutputSlot(int i)
{
	if (i >= (int)outputs.size())
		return NULL;
	return outputs[i];
}

int LiteGraph::LGraphNode::findInputSlotIndex(std::string& name)
{
	for (int i = 0; i < inputs.size(); ++i)
	{
		LSlot* slot = inputs[i];
		if (slot->name == name)
			return i;
	}
	return -1;
}

int LiteGraph::LGraphNode::findOutputSlotIndex(std::string& name)
{
	for (int i = 0; i < outputs.size(); ++i)
	{
		LSlot* slot = outputs[i];
		if (slot->name == name)
			return i;
	}
	return -1;
}

LiteGraph::LGraphNode* LiteGraph::LGraphNode::getInputNode(int slot_index)
{
	assert(graph && slot_index < inputs.size());
	LSlot* slot = outputs[slot_index];
	if (!slot->link)
		return NULL;
	return graph->getNodeById(slot->link->origin_id);
}

std::vector<LiteGraph::LGraphNode*> LiteGraph::LGraphNode::getOutputNodes(int slot_index)
{
	assert(graph && slot_index < outputs.size() );
	std::vector<LiteGraph::LGraphNode*> result;
	LSlot* slot = outputs[slot_index];
	for (int j = 0; j < slot->links.size(); ++j)
	{
		LLink* link = slot->links[j];
		LGraphNode* target = graph->getNodeById(link->target_id);
		result.push_back(target);
	}
	return result;
}

bool LiteGraph::LGraphNode::isInputConnected(int index)
{
	LSlot* slot = getInputSlot(index);
	if (!slot)
		return false;
	return slot->isConnected();
}

bool LiteGraph::LGraphNode::isOutputConnected(int index)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return false;
	return slot->isConnected();
}

void LiteGraph::LGraphNode::connect(int output_slot, LiteGraph::LGraphNode* target_node, int target_input_slot) {
	//Create the new link
	assert(target_node != nullptr && "Target node is nullptr");

	auto* source = outputs[output_slot];
	auto* target = target_node->inputs[target_input_slot];

	assert(source != nullptr && "Source slot index not found");
	assert(target != nullptr && "Targfet slot index not found");

	LLink* link = new LiteGraph::LLink(++graph->last_link_id, id, output_slot, target_node->id, target_input_slot);

	//Persist changes
	graph->links.push_back(link);
	graph->links_by_id[id] = link;
	source->links.push_back(link);
	target->link = link;

	//Log
	if (LiteGraph::verbose) {
		const char* link_type = LiteGraph::typeToString(target->type);
		std::cout << id << " -> " << target_node->id << " [" << (link_type ? link_type : "*") << "]" << std::endl;
	}

}

void LiteGraph::LGraphNode::disconnectInput(int input_slot) {
	auto* slot = inputs[input_slot];
	assert(slot != nullptr && "Slot index not found");

	{//Remove from graph
		graph->links_by_id.erase(slot->link->id);
		auto it = std::find(graph->links.begin(), graph->links.end(), slot->link);
		if (it != graph->links.end())
			graph->links.erase(it);
	}

	{//Remove from source
		auto* link_source_slot = graph->getNodeById(slot->link->origin_id)->getOutputSlot(slot->link->origin_slot);
		auto it = std::find(link_source_slot->links.begin(), link_source_slot->links.end(), slot->link);
		if (it != link_source_slot->links.end())
			link_source_slot->links.erase(it);
	}

	{//Remove from target
		auto* link_target_slot = graph->getNodeById(slot->link->target_id)->getInputSlot(slot->link->target_slot);
		delete link_target_slot->link;
		link_target_slot->link = nullptr;
	}
}

void LiteGraph::LGraphNode::disconnectOutput(int output_slot) {
	//TODO::do not use?
}

LiteGraph::LData* LiteGraph::LGraphNode::getInputData(int index)
{
	LSlot* slot = getInputSlot(index);
	if (!slot)
		return NULL;
	return slot->getOriginData();
}

bool LiteGraph::LGraphNode::getInputDataAsBoolean(int index)
{
	LSlot* slot = getInputSlot(index);
	if (!slot)
		return false;
	LData* data = slot->getOriginData();
	if (data == NULL)
		return false;
	if (data->type == DataType::BOOL)
		return data->boolean;
	return false;
}

double LiteGraph::LGraphNode::getInputDataAsNumber(int index)
{
	LSlot* slot = getInputSlot(index);
	if (!slot)
		return 0;
	LData* data = slot->getOriginData();
	if (data == NULL)
		return 0;
	if (data->type == DataType::NUMBER)
		return data->number;
	return 0;
}

std::string LiteGraph::LGraphNode::getInputDataAsString(int index)
{
	LSlot* slot = getInputSlot(index);
	if (!slot)
		return "";
	LData* data = slot->getOriginData();
	if (data == NULL)
		return "";
	if (data->type == DataType::STRING)
		return data->getString();
	else if (data->type == DataType::NUMBER)
	{
		std::ostringstream ss;
		ss << data->number;
		std::string s(ss.str());
		return s;
	}
	return "";
}

LiteGraph::JSON LiteGraph::LGraphNode::getInputDataAsJSON(int index)
{
	LSlot* slot = getInputSlot(index);
	if (!slot)
		return NULL;
	LData* data = slot->getOriginData();
	if (data == NULL)
		return NULL;
	if (data->type == DataType::JSON_OBJECT)
		return data->custom_data;
	return NULL;
}

/*
template<typename T>
void LiteGraph::LGraphNode::setOutputData(int index, const T& data)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || (slot->type != dataToType(T) && slot->type != DataType::ANY))
	{
		std::cout << "output data dont match: " << slot->type << " expected " << datatypes[ dataToType(T) ] << std::endl;
		return;
	}
	data->assign(data);
}
*/


void LiteGraph::LGraphNode::setOutputDataAsBoolean(int index, const bool& v)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || (slot->type != DataType::BOOL && slot->type != DataType::ANY))
	{
		std::cout << "output data dont match: " << slot->type << " expected BOOLEAN " << std::endl;
		return;
	}
	data->assign(v);
}

void LiteGraph::LGraphNode::setOutputDataAsNumber(int index, const int& v)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || (slot->type != DataType::NUMBER && slot->type != DataType::ANY))
	{
		std::cout << "output data dont match: " << slot->type << " expected NUMBER " << std::endl;
		return;
	}
	data->assign(v);
}

void LiteGraph::LGraphNode::setOutputDataAsNumber(int index, const float& v)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || (slot->type != DataType::NUMBER && slot->type != DataType::ANY))
	{
		std::cout << "output data dont match: " << slot->type << " expected NUMBER " << std::endl;
		return;
	}
	data->assign(v);
}

void LiteGraph::LGraphNode::setOutputDataAsNumber(int index, const double& v)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || (slot->type != DataType::NUMBER && slot->type != DataType::ANY))
	{
		std::cout << "output data dont match: " << slot->type << " expected NUMBER " << std::endl;
		return;
	}
	data->assign(v);
}


void LiteGraph::LGraphNode::setOutputDataAsString(int index, const char* v)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || (slot->type != DataType::STRING && slot->type != DataType::ANY) )
	{
		std::cout << "output data dont match: " << slot->type << " expected STRING " << std::endl;
		return;
	}
	data->assign(v);
}

void LiteGraph::LGraphNode::setOutputDataAsPointer(int index, void* v)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || slot->type != DataType::POINTER)
	{
		std::cout << "output data dont match: " << slot->type << " expected POINTER " << std::endl;
		return;
	}
	data->assign(v);
}

void LiteGraph::LGraphNode::setOutputDataAsEvent(int index, const LEvent& event)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || (slot->type != DataType::EVENT && slot->type != DataType::ANY))
	{
		std::cout << "output data dont match: " << slot->type << " expected EVENT " << std::endl;
		return;
	}
	data->assign(event);
}

void LiteGraph::LGraphNode::trigger(int index, const LEvent& event)
{
	LSlot* slot = getOutputSlot(index);
	if (!slot)
		return;
	LData* data = slot->data;
	if (data == NULL || (slot->type != DataType::EVENT && slot->type != DataType::ANY))
	{
		std::cout << "output data dont match: " << slot->type << " expected EVENT " << std::endl;
		return;
	}
	data->assign(event);

	for (unsigned int i = 0; i < slot->links.size(); ++i)
	{
		LLink* link = slot->links[i];
		LGraphNode* target_node = graph->getNodeById(link->target_id);
		if (!target_node)
			continue; //???
		target_node->onAction( link->target_slot, event );
	}
}


void LiteGraph::LGraphNode::configure(void* json_object)
{
	cJSON *json = (cJSON *)json_object;

	//common vars
	readJSONNumber( json, "order", order );

	//node vars
	onConfigure(json);
}

void LiteGraph::LGraphNode::serialize() {}

void LiteGraph::LGraphNode::removeSlots()
{
	for (unsigned int i = 0; i < inputs.size(); ++i)
		delete inputs[i];
	for (unsigned int i = 0; i < outputs.size(); ++i)
		delete outputs[i];
	inputs.clear();
	outputs.clear();
}

LiteGraph::LLink::LLink(int id, int origin_id, int origin_slot, int target_id, int target_slot)
{
	this->id = id;
	this->origin_id = origin_id;
	this->origin_slot = origin_slot;
	this->target_id = target_id;
	this->target_slot = target_slot;
}

LiteGraph::LGraph::LGraph()
{
	id = 0;
	last_node_id = 0;
	last_link_id = 0;
	has_errors = false;
	custom_data = NULL;
}

LiteGraph::LGraph::~LGraph()
{
	for (int i = 0; i < nodes.size(); ++i)
		delete nodes[i];
	nodes.clear();
}

void LiteGraph::LGraph::add(LGraphNode* node)
{
	assert(node->graph == NULL && "node already belongs to a graph");
	node->graph = this;
	nodes.push_back(node);
	if(node->id == -1)
		node->id = last_node_id++;
	node->order = node->id;
	nodes_by_id[ node->id ] = node;
}

LiteGraph::LGraphNode* LiteGraph::LGraph::getNodeById(int id)
{
	auto it = nodes_by_id.find(id);
	if (it == nodes_by_id.end())
		return NULL;
	return it->second;
}

void LiteGraph::LGraph::clear()
{
	has_errors = false;

	//free
	for (unsigned int i = 0; i < nodes.size(); ++i)
		delete nodes[i];
	for (unsigned int i = 0; i < links.size(); ++i)
		delete links[i];

	//empty
	nodes.clear();
	nodes_by_id.clear();
	links.clear();
	links_by_id.clear();
	nodes_in_execution_order.clear();
	last_link_id = 0;
	last_node_id = 0;
	outputs.clear();
}

void LiteGraph::LGraph::runStep(float dt)
{
	for (unsigned int i = 0; i < nodes_in_execution_order.size(); ++i)
	{
		LGraphNode* node = nodes_in_execution_order[i];
		node->onExecute();
	}
	time += dt;
}

bool comp_func(LiteGraph::LGraphNode* a, LiteGraph::LGraphNode* b)
{
	return a->order < b->order;
}

void LiteGraph::LGraph::sortByExecutionOrder()
{
	nodes_in_execution_order = std::vector<LGraphNode*>( nodes.begin(), nodes.end() );
	std::sort(nodes_in_execution_order.begin(), nodes_in_execution_order.end(), comp_func);
}

void LiteGraph::LGraph::setOutput( std::string name, LiteGraph::LData* data )
{
	outputs[name] = data;
}

bool LiteGraph::LGraph::configure( std::string data )
{
	cJSON *json = cJSON_Parse(data.c_str());
	if (json == NULL)
	{
		std::cerr << "error in JSON file" << std::endl;
		return false;
	}

	cJSON* num_json = cJSON_GetObjectItemCaseSensitive( json, "last_node_id");
	if (cJSON_IsNumber(num_json))
		last_node_id = num_json->valueint;

	num_json = cJSON_GetObjectItemCaseSensitive(json, "last_link_id");
	if (cJSON_IsNumber(num_json))
		last_link_id = num_json->valueint;

	if(LiteGraph::verbose)
		std::cout << "Nodes *****************" << std::endl;

	//nodes
	cJSON* nodes_json = cJSON_GetObjectItemCaseSensitive(json, "nodes");
	cJSON* node_json;
	cJSON_ArrayForEach(node_json, nodes_json)
	{
		int id = cJSON_GetObjectItem(node_json, "id")->valueint;
		char* node_type = cJSON_GetObjectItem(node_json, "type")->valuestring;

		if (LiteGraph::verbose)
			std::cout << id << ".-" << (node_type ? node_type : "?") << std::endl;

		LGraphNode* node = createNode(node_type);
		if (!node)
			node = new LGraphNode(); //create some base node
		node->id = id;
    if (cJSON_HasObjectItem(node_json, "pos")) {
        cJSON* pos_arr   = cJSON_GetObjectItem(node_json, "pos");
        node->position.x = cJSON_GetArrayItem(pos_arr, 0)->valueint;
        node->position.y = cJSON_GetArrayItem(pos_arr, 1)->valueint;
    }

    if (cJSON_HasObjectItem(node_json, "size")) {
        cJSON* size_arr = cJSON_GetObjectItem(node_json, "size");
        node->size.x    = cJSON_GetArrayItem(size_arr, 0)->valueint;
        node->size.y    = cJSON_GetArrayItem(size_arr, 1)->valueint;
    }

		//configure slots as they are in the json
		node->removeSlots();
		cJSON* slot_json = NULL;
		cJSON* inputs_json = cJSON_GetObjectItemCaseSensitive(node_json, "inputs");
		cJSON_ArrayForEach(slot_json, inputs_json)
		{
			char* slot_name = cJSON_GetObjectItem(slot_json, "name")->valuestring;
			DataType slot_type = DataType::ANY;
			cJSON* slot_type_json = cJSON_GetObjectItem(slot_json, "type");
			if (slot_type_json)
			{
				if( slot_type_json->type == 16 ) //string
					slot_type = stringToType(slot_type_json->valuestring);
				else if (slot_type_json->type == 8) //number
					slot_type = slot_type_json->valueint == -1 ? DataType::EVENT : DataType::ANY;
			}
			node->addInput(slot_name,slot_type);
		}

		cJSON* outputs_json = cJSON_GetObjectItemCaseSensitive(node_json, "outputs");
		cJSON_ArrayForEach(slot_json, outputs_json)
		{
			char* slot_name = cJSON_GetObjectItem(slot_json, "name")->valuestring;
			DataType slot_type = DataType::ANY;
			cJSON* slot_type_json = cJSON_GetObjectItem(slot_json, "type");
			if (slot_type_json)
			{
				if (slot_type_json->type == 16) //string
					slot_type = stringToType(slot_type_json->valuestring);
				else if (slot_type_json->type == 8) //number
					slot_type = slot_type_json->valueint == -1 ? DataType::EVENT : DataType::ANY;
			}
			node->addOutput(slot_name, slot_type);
		}

    //hex2rgb
    if (cJSON_HasObjectItem(node_json, "boxcolor")) {
        char* hexcolor = cJSON_GetObjectItem(node_json, "boxcolor")->valuestring;
        node->color = hex2rgb(hexcolor);
    }
    else{
        vec3 defaultcolor = { 11,109,191 };
        node->color = defaultcolor;
    }

    // configure internal node info
    node->configure(node_json);

		//add to graph
		add(node);
	}

	//in case they are not stored in execution order
	sortByExecutionOrder();

	if (LiteGraph::verbose)
		std::cout << "Links *****************" << std::endl;
	
	//connect links
	cJSON* links_json = cJSON_GetObjectItemCaseSensitive(json, "links");
	cJSON* link_json;
	cJSON_ArrayForEach(link_json, links_json)
	{
		int id = cJSON_GetArrayItem(link_json, 0)->valueint;
		int origin_id = cJSON_GetArrayItem(link_json, 1)->valueint;
		int origin_slot_index = cJSON_GetArrayItem(link_json, 2)->valueint;
		int target_id = cJSON_GetArrayItem(link_json, 3)->valueint;
		int target_slot_index = cJSON_GetArrayItem(link_json, 4)->valueint;
		char* link_type = cJSON_GetArrayItem(link_json, 5)->valuestring;

		if (LiteGraph::verbose)
			std::cout << origin_id << " -> " << target_id << " ["<< ( link_type ? link_type : "*" ) << "]" << std::endl;

		LLink* link = new LLink(id, origin_id, origin_slot_index, target_id, target_slot_index);
		links.push_back(link);
		links_by_id[id] = link;

		LGraphNode* origin_node = getNodeById(link->origin_id);
		LGraphNode* target_node = getNodeById(link->target_id);
		if (!origin_node || !target_node)
		{
			std::cerr << "Node not found by its id" << std::endl;
			return false;
		}

		LSlot* origin_slot = origin_node->getOutputSlot(link->origin_slot);
		LSlot* target_slot = target_node->getInputSlot(link->target_slot);
		if (!origin_slot || !target_slot)
		{
			std::cerr << "Nodes slot not found" << std::endl;
			return false;
		}

		origin_slot->links.push_back(link);
		target_slot->link = link;
	}

	if (LiteGraph::verbose)
		std::cout << "***********************" << std::endl;
		
	return true;
}

std::string LiteGraph::LGraph::serialize() {
  cJSON* json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "last_node_id", last_node_id);
  cJSON_AddNumberToObject(json, "last_link_id", last_link_id);

  auto* nodes_arr = cJSON_AddArrayToObject(json, "nodes");
  for (auto* node : nodes) {
    // nodes; {id, type, pos, size, flags{}, order, mode, inputs[{name, type,
    // link}], outputs[{name, type, links}], properties:{}, boxcolor:hex }
    cJSON* node_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(node_json, "id", node->id);
    cJSON_AddStringToObject(node_json, "type", node->getType());

    auto* pos_arr = cJSON_AddArrayToObject(node_json, "pos");
    cJSON_AddItemToArray(pos_arr, cJSON_CreateNumber(node->position.x));
    cJSON_AddItemToArray(pos_arr, cJSON_CreateNumber(node->position.y));

    auto* size_arr = cJSON_AddArrayToObject(node_json, "size");
    cJSON_AddItemToArray(size_arr, cJSON_CreateNumber(node->size.x));
    cJSON_AddItemToArray(size_arr, cJSON_CreateNumber(node->size.y));

    auto* flags_obj = cJSON_AddObjectToObject(node_json, "flags");
    /*for (auto flag : node->flags)
    {}*/

    cJSON_AddNumberToObject(node_json, "order", node->order);
    cJSON_AddNumberToObject(node_json, "mode", 0);//?

    if (node->inputs.size()) 
    {
        auto* inputs_arr = cJSON_AddArrayToObject(node_json, "inputs");
        for (auto* input : node->inputs) {
          cJSON* input_json = cJSON_CreateObject();

          cJSON_AddStringToObject(input_json, "name", input->name.c_str());
          cJSON_AddStringToObject(input_json, "type",
                                  LiteGraph::typeToString(input->type));
          if (input->link)
            cJSON_AddNumberToObject(input_json, "link", input->link->id);
          else
            cJSON_AddNullToObject(input_json, "link");

          cJSON_AddItemToArray(inputs_arr, input_json);
        }
    }

    if (node->outputs.size())
    {
        auto* outputs_arr = cJSON_AddArrayToObject(node_json, "outputs");
        for (auto* output : node->outputs) {
          cJSON* output_json = cJSON_CreateObject();

          cJSON_AddStringToObject(output_json, "name", output->name.c_str());
          cJSON_AddStringToObject(output_json, "type",
                                  LiteGraph::typeToString(output->type));
          auto* links_arr = cJSON_AddArrayToObject(output_json, "links");
          for (auto* link : output->links)
            cJSON_AddItemToArray(links_arr, cJSON_CreateNumber(link->id));

          cJSON_AddItemToArray(outputs_arr, output_json);
        }
    }

    auto* properties_obj = cJSON_AddObjectToObject(node_json, "properties");
    // node properties missing?

    std::string hexcolor = rgb2hex(node->color);
    cJSON_AddStringToObject(node_json, "boxcolor", hexcolor.c_str());

    // serialize internal node info
    // node->serialize(node_json);

    cJSON_AddItemToArray(nodes_arr, node_json);
  }

  cJSON* links_arr = cJSON_AddArrayToObject(json, "links");
  for (auto* link : links) {
    // links; array of [id, origin_node, origin_slot, target_node, target_slot,
    // type]
    cJSON* link_arr = cJSON_CreateArray();

    auto* node = nodes_by_id[link->target_id];
    auto* slot = node->inputs[link->target_slot];
    cJSON_AddItemToArray(link_arr, cJSON_CreateNumber(link->id));
    cJSON_AddItemToArray(link_arr, cJSON_CreateNumber(link->origin_id));
    cJSON_AddItemToArray(link_arr, cJSON_CreateNumber(link->origin_slot));
    cJSON_AddItemToArray(link_arr, cJSON_CreateNumber(link->target_id));
    cJSON_AddItemToArray(link_arr, cJSON_CreateNumber(link->target_slot));
    cJSON_AddItemToArray(
        link_arr, cJSON_CreateString(LiteGraph::typeToString(slot->type)));

    cJSON_AddItemToArray(links_arr, link_arr);
  }

  // groups[]
  // config{}
  // version: number

  std::string string = cJSON_Print(json);
  return string;
}

// utils
std::string LiteGraph::getFileContent(const std::string& path) {
	std::ifstream file(path);
	
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return content;
}

//std::map<std::string, LiteGraph::LGraphNode*> LiteGraph::node_types;

void LiteGraph::registerNodeType(LiteGraph::LGraphNode* node_type)
{
	node_types[node_type->getType()] = node_type;
	if(LiteGraph::verbose)
		std::cout << "node registered: " << node_type->getType() << " ******************* " << std::endl;
}

LiteGraph::LGraphNode* LiteGraph::createNode(const char* name)
{
	auto it = node_types.find(name);
	if (it == node_types.end())
	{
		std::cerr << "node type not found: " << name << std::endl;
		return NULL;
	}

	return it->second->clone();
}


bool LiteGraph::readJSONBoolean( JSON obj, const char* name, bool& dst)
{
	cJSON* value_json = cJSON_GetObjectItemCaseSensitive((cJSON*)obj, name);
	if (!value_json)
		return false;
	if (cJSON_IsBool(value_json))
		dst = value_json->valueint == 1;
	else
		return false;
	return true;
}

bool LiteGraph::readJSONNumber(JSON obj, const char* name, int& dst)
{
	cJSON* value_json = cJSON_GetObjectItemCaseSensitive((cJSON*)obj, name);
	if (!value_json)
		return false;
	if (cJSON_IsNumber(value_json))
		dst = value_json->valueint;
	else
		return false;
	return true;
}

bool LiteGraph::readJSONNumber(JSON obj, const char* name, float& dst)
{
	cJSON* value_json = cJSON_GetObjectItemCaseSensitive((cJSON*)obj, name);
	if (!value_json)
		return false;
	if (cJSON_IsNumber(value_json))
		dst = (float)value_json->valuedouble;
	else
		return false;
	return true;
}

bool LiteGraph::readJSONNumber(JSON obj, const char* name, double& dst)
{
	cJSON* value_json = cJSON_GetObjectItemCaseSensitive((cJSON*)obj, name);
	if (!value_json)
		return false;
	if (cJSON_IsNumber(value_json))
		dst = value_json->valuedouble;
	else
		return false;
	return true;
}

bool LiteGraph::readJSONString(JSON obj, const char* name, std::string& dst)
{
	cJSON* value_json = cJSON_GetObjectItemCaseSensitive((cJSON*)obj, name);
	if (!value_json)
		return false;
	if (cJSON_IsString(value_json))
		dst = value_json->valuestring;
	else
		return false;
	return true;
}

LiteGraph::JSON LiteGraph::getJSONObject(JSON obj, const char* name)
{
	return (void*)cJSON_GetObjectItemCaseSensitive((cJSON*)obj, name);
}

LiteGraph::JSON LiteGraph::parseJSON(const char* str)
{
	cJSON *json = cJSON_Parse(str);
	if (json == NULL)
		std::cerr << "Error parsing JSON: " << str << std::endl;
	return json;
}

void LiteGraph::freeJSON(JSON json)
{
	cJSON_Delete((cJSON*)json);
}

std::map<std::string, int> custom_data_types;

std::string capitalize(std::string str)
{
	std::string result = str;
	for (unsigned int i = 0; i < result.size(); ++i)
		result[i] = toupper(result[i]);
	return result;
}

void LiteGraph::registerCustomDataType(const char* name, int id)
{
	std::string s = name;
	s = capitalize(s);
	custom_data_types[s] = id;
}

LiteGraph::DataType LiteGraph::stringToType(const char* str)
{
	std::string s = str;
	s = capitalize(s);

	//custom
	auto it = custom_data_types.find( s );
	if (it != custom_data_types.end())
		return (DataType)it->second;

	//default
	if(s == "ENUM")
		return DataType::ENUM;
	if (s == "NUMBER")
		return DataType::NUMBER;
	if (s == "STRING")
		return DataType::STRING;
	if (s == "BOOL" || s == "BOOLEAN")
		return DataType::BOOL;
	if (s == "VEC2")
		return DataType::VEC2;
	if (s == "VEC3")
		return DataType::VEC3;
	if (s == "MAT4")
		return DataType::MAT4;
	if (s == "OBJECT")
		return DataType::OBJECT;
	if (s == "ARRAY")
		return DataType::ARRAY;
	if (s == "JSON_OBJECT")
		return DataType::JSON_OBJECT;
	if (s == "POINTER")
		return DataType::POINTER;
	if (s == "EVENT")
		return DataType::EVENT;
	if (s == "" || s == "*")
		return DataType::ANY;

	return DataType::CUSTOM;
}

const char* LiteGraph::typeToString(LiteGraph::DataType type)
{
	switch (type)
	{
	case DataType::ENUM:		return "ENUM";
	case DataType::NUMBER:		return "NUMBER";
	case DataType::STRING:		return "STRING";
	case DataType::BOOL:		return "BOOL";
	case DataType::VEC2:		return "VEC2";
	case DataType::VEC3:		return "VEC3";
	case DataType::MAT4:		return "MAT4";
	case DataType::OBJECT:		return "OBJECT";
	case DataType::ARRAY:		return "ARRAY";
	case DataType::JSON_OBJECT:	return "JSON_OBJECT";
	case DataType::POINTER:		return "POINTER";
	case DataType::EVENT:		return "EVENT";
	case DataType::ANY:			return "*";
	default: return "*";
	}
}

void LiteGraph::init()
{
	//register base nodes in the system
	initBaseNodes();
}